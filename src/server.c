#include <llhttp.h>
#include <mserv.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>

volatile sig_atomic_t g_mserv_event_loop_running = 1;
volatile sig_atomic_t g_mserv_stop_signal = 0;

static void mserv_sighandler(int sig)
{
        if (sig == SIGTERM || sig == SIGINT) {
                g_mserv_stop_signal = sig;
                g_mserv_event_loop_running = 0;
        }
}

static size_t mserv_get_maxfd(void)
{
        struct rlimit rl;

        if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
                return 0;

        return rl.rlim_cur;
}

static int mserv_import_modules(struct mserv_module *modules_head, size_t max)
{
        struct dirent *entry;
        DIR *dir;
        size_t count = 0;
        
        dir = opendir(MSERV_MODULES_FOLDER);
        if (!dir)
                mserv_warn_ret(0, "Modules folder not found\n");

        while (count < max && (entry = readdir(dir))) {
                size_t len;
                
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                len = strlen(entry->d_name);

                if (len < literal_len(".so"))
                        continue;

                if (strcmp(entry->d_name + (len - literal_len(".so")), ".so") == 0) {
                        char full_path[literal_len(MSERV_MODULES_FOLDER) + 255 + 2];
                        strcpy(full_path, MSERV_MODULES_FOLDER);
                        strcat(full_path, "/");
                        strcat(full_path, entry->d_name);

                        if (mserv_module_import(modules_head, full_path) < 0)
                                mserv_warn("Failed to initialize the module '%s'\n", entry->d_name);
                        else
                                mserv_log("Module '%s' successfully initialized\n", entry->d_name);

                        count++;
                }
        }

        closedir(dir);
        return 0;
}

static unsigned int mserv_get_count_modules(void)
{
        struct dirent *entry;
        size_t count = 0;
        DIR *dir;
        
        dir = opendir(MSERV_MODULES_FOLDER);
        if (!dir)
                mserv_warn_ret(0, "Modules folder not found\n");

        while ((entry = readdir(dir))) {
                size_t len;
                
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                len = strlen(entry->d_name);

                if (len < literal_len(".so"))
                        continue;

                if (strcmp(entry->d_name + (len - literal_len(".so")), ".so") == 0) {
                        count++;
                }
        }

        closedir(dir);
        return count;
}

static int mserv_init_ssl(struct mserver *serv)
{
        serv->ssl_method = TLS_server_method();
        if (!serv->ssl_method)
                mserv_err_ret(-1, "Failed to initialize TLS server method\n");

        serv->ssl_ctx = SSL_CTX_new(serv->ssl_method);
        if (!serv->ssl_ctx)
                mserv_err_ret(-1, "Failed to initialize SSL CTX to server\n");

        SSL_CTX_set_options(serv->ssl_ctx, SSL_OP_ENABLE_KTLS);
        
        if (SSL_CTX_use_certificate_file(serv->ssl_ctx,
                MSERV_TLS_CERTIFICATE_FOLDER "/" MSERV_TLS_CERTIFICATE_NAME ".crt", SSL_FILETYPE_PEM) <= 0) {
                        SSL_CTX_free(serv->ssl_ctx);
                        mserv_err_ret(-1, "Failed to create SSL CTX certificate file for server\n");
        }

        if (SSL_CTX_use_PrivateKey_file(serv->ssl_ctx,
                MSERV_TLS_CERTIFICATE_FOLDER "/" MSERV_TLS_CERTIFICATE_NAME ".key", SSL_FILETYPE_PEM) <= 0) {
                        SSL_CTX_free(serv->ssl_ctx);
                        mserv_err_ret(-1, "Failed to create SSL CTX private key file for server\n");
        }

        return 0;
}

static void mserv_destroy_ssl(struct mserver *serv)
{
        if (serv->ssl_ctx)
                SSL_CTX_free(serv->ssl_ctx);
}

static int mserv_modules_init(struct mserv_modules *modules_data, size_t mods_count)
{
        modules_data->modules_head = mobj_pool_alloc(&g_mserv_obj.mserv_module_pool);
        if (!modules_data->modules_head)
                mserv_err_ret(-1, "Failed to allocate the server module object in the pool\n");

        mlist_head_init(&modules_data->modules_head->list);

        if (mserv_import_modules(modules_data->modules_head, mods_count) < 0)
                mserv_err_goto(fail, "Failed to initialize the server API modules\n");

        pthread_mutex_init(&modules_data->mutex, NULL);

        if (pthread_create(&modules_data->watcher, NULL, mserv_module_watch_worker, NULL) < 0)
                mserv_err_goto(fail, "Failed to create thread for module watcher worker\n");

        pthread_detach(modules_data->watcher);

        return 0;
fail:
        if (modules_data->modules_head) {
                for (struct mlist_head *l = modules_data->modules_head->list.next; l != &modules_data->modules_head->list; ) {
                        struct mserv_module *mod = container_of(l, struct mserv_module, list);
                        l = l->next;
                        
                        mserv_module_destroy(mod);
                        mobj_pool_free(&g_mserv_obj.mserv_module_pool, mod);
                }

                mserv_module_destroy(modules_data->modules_head);
                mobj_pool_free(&g_mserv_obj.mserv_module_pool, modules_data->modules_head);
        }

        pthread_mutex_destroy(&modules_data->mutex);

        return -1;
}

static void mserv_modules_destroy(struct mserv_modules *modules_data)
{
        pthread_cancel(modules_data->watcher);
        pthread_mutex_destroy(&modules_data->mutex);

        for (struct mlist_head *l = modules_data->modules_head->list.next; l != &modules_data->modules_head->list; ) {
                struct mserv_module *mod = container_of(l, struct mserv_module, list);
                l = l->next;

                mserv_module_destroy(mod);
                mobj_pool_free(&g_mserv_obj.mserv_module_pool, mod);
        }

        mserv_module_destroy(modules_data->modules_head);
        mobj_pool_free(&g_mserv_obj.mserv_module_pool, modules_data->modules_head);
}

int mserv_init(struct mserver *serv)
{
        struct epoll_event ev;
        int sockoptval;
        size_t mods_count;

        if (!serv)
                return -1;

        memset(serv, 0, sizeof(struct mserver));
        serv->servfd = -1;
        serv->epfd = -1;
        mserv_time_update();
        mods_count = mserv_get_count_modules();

        if (mserv_init_ssl(serv) < 0)
                mserv_err_ret(-1, "Failed to ssl initialize\n");

        if (mobj_pool_init(&serv->mhttp_request_pool, sizeof(struct mhttp_request), MSERV_START_MAX_CONNECTIONS) < 0)
                mserv_err_goto(fail, "Failed to create object pool for request struct\n");
        if (mobj_pool_init(&serv->mhttp_response_pool, sizeof(struct mhttp_response), MSERV_START_MAX_CONNECTIONS) < 0)
                mserv_err_goto(fail, "Failed to create object pool for response struct\n");
        if (mobj_pool_init(&serv->mserv_module_pool, sizeof(struct mserv_module), mods_count) < 0)
                mserv_err_goto(fail, "Failed to create object pool for server module struct\n");

        serv->servfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serv->servfd < 0)
                mserv_err_goto(fail, "Failed to create server socket\n");

        sockoptval = 1;

        if (setsockopt(serv->servfd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(sockoptval)) < 0)
                mserv_err_goto(fail, "Failed to set parameters for the server socket\n");

        fd_set_nonblock(serv->servfd);

        serv->conns_capacity = mserv_get_maxfd();
        if (!serv->conns_capacity)
                mserv_err_goto(fail, "Failed to set the maximum number of descriptors that can be connected to the server\n");

        serv->conns = calloc(serv->conns_capacity, sizeof(struct mconnection));
        if (!serv->conns)
                mserv_err_goto(fail, "Memory allocation error\n");

        for (size_t i = 0; i < serv->conns_capacity; i++)
                serv->conns[i].fd = -1;

        serv->epfd = epoll_create1(0);
        if (serv->epfd < 0)
                mserv_err_goto(fail, "Failed to create 'event poll'\n");

        serv->addr.sin_port = htons(MSERV_PORT);
        serv->addr.sin_family = AF_INET;
        serv->addr.sin_addr.s_addr = INADDR_ANY;
        
        serv->addrlen = sizeof(struct sockaddr_in);

        if (bind(serv->servfd, (struct sockaddr*)&serv->addr, serv->addrlen) < 0)
                mserv_err_goto(fail, "Failed to bind the address to the server\n");

        if (listen(serv->servfd, MSERV_BACKLOG_VAL) < 0)
                mserv_err_goto(fail, "Failed to start client listening\n");

        ev.events = MSERV_EPOLL_EVENTS;
        ev.data.ptr = serv;
        if (epoll_ctl(serv->epfd, EPOLL_CTL_ADD, serv->servfd, &ev) < 0)
                mserv_err_goto(fail, "Failed to add the server socket to the 'event poll'\n");

        mparser_settings_init(&serv->parser_settings);

        if (mserv_hm_init(&serv->mhttp_route_map, mods_count / 0.75 + 1) < 0)
                mserv_err_goto(fail, "Failed to initialize the routing hash table\n");

        if (mserv_modules_init(&serv->modules_data, mods_count) < 0)
                mserv_err_goto(fail, "Failed to initialize the server modules\n");

        return 0;
fail:
        if (serv->servfd >= 0)
                close(serv->servfd);
        if (serv->conns)
                free(serv->conns);
        if (serv->epfd >= 0)
                close(serv->epfd);
        if (serv->mhttp_route_map.capacity)
                mserv_hm_destroy(&serv->mhttp_route_map);
        if (serv->ssl_ctx)
                SSL_CTX_free(serv->ssl_ctx);
        if (serv->mhttp_request_pool.total_alloc)
                mobj_pool_destroy(&serv->mhttp_request_pool);
        if (serv->mhttp_response_pool.total_alloc)
                mobj_pool_destroy(&serv->mhttp_response_pool);
        if (serv->mserv_module_pool.total_alloc)
                mobj_pool_destroy(&serv->mserv_module_pool);

        return -1;
}

void mserv_destroy(struct mserver *serv)
{
        if (!serv)
                return;

        if (serv->conns) {
                for (size_t i = 0; i < serv->conns_capacity; i++) {
                        if (serv->conns[i].fd < 0)
                                continue;

                        mconn_destroy(&serv->conns[i]);
                }

                free(serv->conns);
                mserv_log("All connections was destroyed\n");
        }

        mserv_modules_destroy(&serv->modules_data);
        mserv_log("All modules was destroyed\n");

        mobj_pool_destroy(&serv->mhttp_request_pool);
        mobj_pool_destroy(&serv->mhttp_response_pool);
        mobj_pool_destroy(&serv->mserv_module_pool);
        mserv_log("All object pools was destroyed\n");

        mserv_destroy_ssl(serv);
        mserv_log("Server TLS was destroyed\n");

        serv->conns_capacity = 0;
        serv->conns_count = 0;

        if (serv->epfd >= 0) {
                close(serv->epfd);
                mserv_log("Server EPOLL descriptor was closed\n");
        }
        if (serv->servfd >= 0) {
                close(serv->servfd);
                mserv_log("Server socket was closed\n");
        }

        serv->epfd = -1;
        serv->servfd = -1;
}

static int mserv_event_loop_accept_all(struct mserver *serv, int *idle_fd)
{
        while (g_mserv_event_loop_running) {
                struct mconnection *elem;
                struct epoll_event mconn_event;
                int client_fd;
                                                
                client_fd = accept(serv->servfd, (struct sockaddr*)&serv->addr, &serv->addrlen);
                if (client_fd < 0) {
                        if (errno == EWOULDBLOCK || errno == EAGAIN) {
                                break;
                        }
                        else if (errno == EMFILE) {
                                close(*idle_fd);

                                client_fd = accept(serv->servfd, (struct sockaddr*)&serv->addr, &serv->addrlen);
                                if (client_fd >= 0)
                                        close(client_fd);

                                *idle_fd = open("/dev/null", O_RDONLY);
                                if (*idle_fd < 0)
                                        mserv_err_goto(fail, "Error opening file: %s\n", strerror(errno));

                                continue;
                        }
                        
                        mserv_warn("Failed to accept the connection\n");
                        continue;
                }

                if ((size_t)client_fd >= serv->conns_capacity) {
                        mserv_warn("Insufficient space to accept the connection. Closing the connection...\n");
                        close(client_fd);
                        continue;
                }

                mserv_log("Client %d accepted\n", client_fd);

                fd_set_nonblock(client_fd);
                elem = &serv->conns[client_fd];

                if (mconn_init(elem, serv, client_fd) < 0) {
                        mserv_warn("Error initializing connection interface. Closing connection...\n");
                        close(client_fd);
                        continue;
                }

                mconn_event.events = MSERV_EPOLL_EVENTS;
                mconn_event.data.ptr = elem;

                if (epoll_ctl(serv->epfd, EPOLL_CTL_ADD, client_fd, &mconn_event) < 0) {
                        mserv_warn("Error adding client to 'event poll'\n");
                        mconn_destroy(elem);
                        continue;
                }
        }

        return 0;
fail:
        return -1;
}

int mserv_event_loop(struct mserver *serv)
{
        int idle_fd;
        struct sigaction sa = {0};

        if (!serv)
                return -1;

        idle_fd = open("/dev/null", O_RDONLY);
        if (idle_fd < 0)
                mserv_err_ret(-1, "Error opening file: %s\n", strerror(errno));

        sa.sa_handler = mserv_sighandler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGPIPE, &sa, NULL);

        while (g_mserv_event_loop_running) {
                struct epoll_event events[MSERV_EPOLL_MAXEVENTS];
                int n;

                n = epoll_wait(serv->epfd, events, MSERV_EPOLL_MAXEVENTS, -1);

                mserv_time_update();

                if (n < 0) {
                        if (errno == EINTR)
                                continue;

                        mserv_err_goto(fail, "Epoll wait error: %s\n", strerror(errno));
                }

                for (int i = 0; i < n; i++) {
                        struct epoll_event *ev = &events[i];
                        
                        if (ev->data.ptr == serv) {
                                if (ev->events & EPOLLIN) {
                                        if (mserv_event_loop_accept_all(serv, &idle_fd) < 0)
                                                goto fail;
                                }
                        }
                        else {
                                struct mconnection *conn = ev->data.ptr;

                                if (ev->events & EPOLLERR || ev->events & EPOLLHUP) {
                                        mserv_warn("%s. Closing connection...\n",
                                                (ev->events & (EPOLLERR | EPOLLHUP)) ? "EPOLLERR EPOLLHUP" :
                                                (ev->events & EPOLLERR) ? "EPOLLERR" : "EPOLLHUP");

                                        mconn_destroy(conn);
                                        continue;
                                }

                                if (ev->events & EPOLLIN || ev->events & EPOLLOUT) {
                                        if (mconn_step(conn) < 0) {
                                                mserv_log("Closing connection...\n");
                                                mconn_destroy(conn);
                                                continue;
                                        }
                                }
                        }
                }
        }

        if (g_mserv_stop_signal)
                mserv_log("Server was stopped by a %s signal\n",
                        (g_mserv_stop_signal == SIGTERM) ? "SIGTERM" : "SIGINT");

        mserv_destroy(serv);
        mserv_log("Server was destroyed\n");

        return 0;
fail:
        mserv_destroy(serv);
        return -1;
}

void mserv_time_update(void)
{
        g_mserv_obj.cached_time = time(NULL);
}