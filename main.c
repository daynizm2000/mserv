#include <mserv.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <openssl/ssl.h>
#include <dlfcn.h>
#include <string.h>

#define MAX_DIR_PROJECT_LEN 255 // base max value file length size in linux

struct mserver g_mserv_obj;

static int project_path_validate(const char *path)
{
        struct stat st;

        if (strlen(path) > MAX_DIR_PROJECT_LEN)
                mserv_log_ret(0, "Invalid project path: '%s'. Exceeding the maximum permissible length\n", path);

        if (lstat(path, &st) != 0) {
                if (errno == ENOENT)
                        mserv_log_ret(0, "Invalid project path: '%s'. No such directory\n", path);
                else if (errno == EACCES)
                        mserv_log_ret(0, "Invalid project path: '%s'. Permission error\n", path);
                else
                        return 0;
        }

        if (!S_ISDIR(st.st_mode))
                mserv_log_ret(0, "Invalid project path: '%s'. Is not directory\n", path);

        return 1;
}

static int project_path_set(const char *path, char *dest)
{
        size_t len;

        if (!project_path_validate(path))
                return -1;

        len = strlen(path);
        strcpy(dest, path);

        if (dest[len] == '/')
                dest[len] = '\0';
        else
                dest[len + 1] = '\0';

        return 0;
}

int main(int argc, char **argv)
{
        char project_path[256];
        mserv_time_update();
        OPENSSL_init_ssl(0, NULL);

        if (argc < 2)
                mserv_log_ret(1, "Use: %s <path to your project>\n", *argv);

        if (project_path_set(argv[1], project_path) < 0)
                return 1;

        if (mserv_init(&g_mserv_obj) < 0)
                return 1;

        g_mserv_obj.project_path = project_path;
        mserv_log("The server has been successfully initialized.\n");

        return -mserv_event_loop(&g_mserv_obj);
}