#include <mserv.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <string.h>

#define THREAD_NAME "module watcher"

void mserv_module_init(struct mserv_module *mod)
{
        memset(mod, 0, sizeof(struct mserv_module));
}

void mserv_module_destroy(struct mserv_module *mod)
{
        if (mod->handle)
                dlclose(mod->handle);

        free((void*)mod->url);
        mlist_del(&mod->list);
        memset(mod, 0, sizeof(struct mserv_module));
}

static int is_module(const char *fname)
{
        size_t len = strlen(fname);

        if (len >= literal_len(".so") && strcmp(fname + (len - literal_len(".so")), ".so") == 0)
                return 1;

        return 0;
}

static int mserv_module_route_register(struct mserv_module *mod,
        const char *url, mhttp_api_handler_t handler)
{
        mod->url = strdup(url);
        if (!mod->url)
                mserv_err_ret(-1, "Memory allocation error\n");

        return mhttp_route_api_register(mod, handler);
}

int mserv_module_import(struct mserv_module *modules_head, const char *path)
{
        struct mserv_module *module = NULL;
        int (*mserv_module_reg_f)(struct mserv_module*);
        void *handle = NULL;
        
        handle = dlopen(path, RTLD_NOW);
        if (!handle)
                mserv_err_ret(-1, "Error opening library '%s'\n", path);

        mserv_module_reg_f = dlsym(handle, "mserv_module_register");
        if (!mserv_module_reg_f)
                mserv_err_goto(fail, "Error connecting function from library '%s'\n", path);

        module = mobj_pool_alloc(&g_mserv_obj.mserv_module_pool);
        if (!module)
                mserv_err_goto(fail, "Failed to allocate the server module object in the pool\n");

        mserv_module_init(module);

        module->handle = handle;
        mlist_add(&modules_head->list, &module->list);

        module->route_register = mserv_module_route_register;
        if (mserv_module_reg_f(module) < 0)
                goto fail;

        strcpy(module->full_path, path);
        return 0;
fail:
        if (handle)
                dlclose(handle);
        if (module) {
                mlist_del(&module->list);
                mserv_module_destroy(module);
                mobj_pool_free(&g_mserv_obj.mserv_module_pool, module);
        }

        return -1;
}

struct mserv_module *mserv_find_module_by_fpath(struct mserv_module *modules_head, const char *full_path)
{
        struct mlist_head *l = &modules_head->list;

        do {
                struct mserv_module *mod = container_of(l, struct mserv_module, list);

                if (strcmp(mod->full_path, full_path) == 0)
                        return mod;

                l = l->next;
        } while (l != &modules_head->list);

        return NULL;
}

void *mserv_module_watch_worker(void *arg)
{
        int wd;
        int fd;
        char buffer[4096];

        (void)arg;

        fd = inotify_init1(0);
        wd = inotify_add_watch(fd, MSERV_MODULES_FOLDER,
                IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);

        while (g_mserv_event_loop_running) {
                size_t event_len = 0;
                ssize_t read_bytes = read(fd, buffer, sizeof(buffer));
                if (read_bytes < 0)
                        mserv_err_goto(cleanup, "[Thread '%s']: Module fd read error\n", THREAD_NAME);
                if (!read_bytes)
                        continue;

                for (char *ptr = buffer; ptr < buffer + read_bytes; ptr += sizeof(struct inotify_event) + event_len) {
                        struct inotify_event *event = (struct inotify_event*)ptr;
                        char full_path[literal_len(MSERV_MODULES_FOLDER) + 255 + 2];

                        event_len += event->len;

                        if (!is_module(event->name))
                                continue;

                        strcpy(full_path, MSERV_MODULES_FOLDER);
                        strcat(full_path, "/");
                        strcat(full_path, event->name);

                        if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                                struct mserv_module *mod;
                                usleep(300000);

                                pthread_mutex_lock(&g_mserv_obj.modules_data.mutex);

                                mod = mserv_find_module_by_fpath(g_mserv_obj.modules_data.modules_head, full_path);
                                if (mod) {
                                        mserv_module_destroy(mod);
                                        mobj_pool_free(&g_mserv_obj.mserv_module_pool, mod);
                                }

                                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);

                                if (mserv_module_import(g_mserv_obj.modules_data.modules_head, full_path) < 0)
                                        mserv_warn("[Thread '%s']: Failed to initialize the module '%s'\n", THREAD_NAME, event->name);
                                else
                                        mserv_log("[Thread '%s']: Module '%s' successfully initialized\n", THREAD_NAME, event->name);
                        }
                        else if (event->mask & IN_DELETE) {
                                struct mserv_module *mod;

                                pthread_mutex_lock(&g_mserv_obj.modules_data.mutex);

                                mod = mserv_find_module_by_fpath(g_mserv_obj.modules_data.modules_head, full_path);
                                if (mod) {
                                        mhttp_route_api_unregister(mod->url);
                                        mserv_module_destroy(mod);
                                        mobj_pool_free(&g_mserv_obj.mserv_module_pool, mod);
                                        mserv_log("[Thread '%s']: Module '%s' successfully removed\n", THREAD_NAME, event->name);
                                }

                                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);
                        }
                }
        }

cleanup:
        inotify_rm_watch(fd, wd);
        close(fd);
        close(wd);
        return NULL;
}