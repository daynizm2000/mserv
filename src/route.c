#include <mserv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void mhttp_route_free(void *key, size_t ksize, void *data)
{
        struct mhttp_route *route = data;

        (void)key;
        (void)ksize;

        free(route);
}

int mhttp_route_api_register(struct mserv_module *mod, mhttp_api_handler_t handler)
{
        struct mhttp_route *route = malloc(sizeof(struct mhttp_route));
        if (!route)
                mserv_err_ret(-1, "Memory allocation error\n");

        route->module = mod;
        route->handler = handler;

        pthread_mutex_lock(&g_mserv_obj.modules_data.mutex);

        if (mserv_hm_add(&g_mserv_obj.mhttp_route_map, mod->url, strlen(mod->url), route, mhttp_route_free) < 0) {                
                free(route);
                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);
                mserv_err_ret(-1, "Failed to register the API in the hash table\n");
        }

        pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);
 
        return 0;
}

void mhttp_route_api_unregister(const char *url)
{
        mserv_hm_delete(&g_mserv_obj.mhttp_route_map, (void*)url, strlen(url));
}