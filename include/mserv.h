#pragma once

#include <llhttp.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <time.h>
#include "../config/config.h"
#include <signal.h>

extern struct mserver g_mserv_obj;
extern volatile sig_atomic_t g_mserv_event_loop_running;
extern volatile sig_atomic_t g_mserv_stop_signal;

#define MSERV_ERR "\033[0;31m[ERR]\033[0m "
#define MSERV_WARN "\033[0;33m[WARN]\033[0m "

#define mserv_log(fmt, ...) fprintf(stderr, "[LOG] [%lu] " fmt, g_mserv_obj.cached_time, ##__VA_ARGS__)
#define mserv_warn(fmt, ...) mserv_log(MSERV_WARN "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define mserv_err(fmt, ...) mserv_log(MSERV_ERR "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define __mserv_log_action(action, show_info, fmt, ...) \
        do { \
                show_info(fmt, ##__VA_ARGS__); \
                action; \
        } while (0)

#define __mserv_log_ret(show_info, ret, fmt, ...) __mserv_log_action(return ret, show_info, fmt, ##__VA_ARGS__)
#define __mserv_log_goto(show_info, point, fmt, ...) __mserv_log_action(goto point, show_info, fmt, ##__VA_ARGS__)

#define mserv_log_ret(ret, fmt, ...) __mserv_log_ret(mserv_log, ret, fmt, ##__VA_ARGS__)
#define mserv_log_goto(point, fmt, ...) __mserv_log_goto(mserv_log, point, fmt, ##__VA_ARGS__)

#define mserv_warn_ret(ret, fmt, ...) __mserv_log_ret(mserv_warn, ret, fmt, ##__VA_ARGS__)
#define mserv_warn_goto(point, fmt, ...) __mserv_log_goto(mserv_warn, point, fmt, ##__VA_ARGS__)

#define mserv_err_ret(ret, fmt, ...) __mserv_log_ret(mserv_err, ret, fmt, ##__VA_ARGS__)
#define mserv_err_goto(point, fmt, ...) __mserv_log_goto(mserv_err, point, fmt, ##__VA_ARGS__)

#define literal_len(str_lit) (sizeof(str_lit) - 1)

#define MSERV_DEFINE_API_MODULE(path, handler) \
        int mserv_module_register(struct mserv_module *mod) { \
                return mod->route_register(mod, (path), (handler)); \
        }

#define MSERV_NAME MCONFIG_SERVER_NAME
#define MSERV_VERSION "0.1"

#define MSERV_START_MAX_CONNECTIONS MCONFIG_START_MAX_CONNECTIONS

#define MSERV_MODULES_FOLDER MCONFIG_MODULES_FOLDER

#define MSERV_TLS_CERTIFICATE_FOLDER MCONFIG_TLS_CERTIFICATE_FOLDER
#define MSERV_TLS_CERTIFICATE_NAME MCONFIG_TLS_CERTIFICATE_NAME

#define MSERV_PORT MCONFIG_SERVER_PORT
#define MSERV_BACKLOG_VAL 128
#define MSERV_EPOLL_MAXEVENTS 1024
#define MSERV_EPOLL_EVENTS (EPOLLERR | EPOLLHUP | EPOLLET | EPOLLIN | EPOLLOUT)

#define MHTTP_HEADER_SIZE 8192
#define MHTTP_MAIN_HTML_PAGE_PATH MCONFIG_HTTP_MAIN_HTML_PAGE_PATH

#define MHTTP_CONT_TYPE_HTML "text/html; charset=utf-8"
#define MHTTP_CONT_TYPE_CSS "text/css; charset=utf-8"
#define MHTTP_CONT_TYPE_JS "text/javascript; charset=utf-8"
#define MHTTP_CONT_TYPE_TEXT "text/plain; charset=utf-8"
#define MHTTP_CONT_TYPE_JSON "application/json"
#define MHTTP_CONT_TYPE_XML "application/xml"
#define MHTTP_CONT_TYPE_JPEG "image/jpeg"
#define MHTTP_CONT_TYPE_PNG "image/png"
#define MHTTP_CONT_TYPE_GIF "image/gif"
#define MHTTP_CONT_TYPE_SVG "image/svg+xml"
#define MHTTP_CONT_TYPE_WEBP "image/webp"
#define MHTTP_CONT_TYPE_XICON "image/x-icon"
#define MHTTP_CONT_TYPE_OCTET_STREAM "application/octet-stream"
#define MHTTP_CONT_TYPE_PDF "application/pdf"
#define MHTTP_CONT_TYPE_ZIP "application/zip"

#define MHTTP_HTML_FOLDER MCONFIG_HTTP_HTML_FOLDER
#define MHTTP_CSS_FOLDER MCONFIG_HTTP_CSS_FOLDER
#define MHTTP_JS_FOLDER MCONFIG_HTTP_JS_FOLDER
#define MHTTP_TXT_FOLDER MCONFIG_HTTP_TXT_FOLDER
#define MHTTP_JSON_FOLDER MCONFIG_HTTP_JSON_FOLDER
#define MHTTP_XML_FOLDER MCONFIG_HTTP_XML_FOLDER
#define MHTTP_JPEG_FOLDER MCONFIG_HTTP_JPEG_FOLDER
#define MHTTP_PNG_FOLDER MCONFIG_HTTP_PNG_FOLDER
#define MHTTP_GIF_FOLDER MCONFIG_HTTP_GIF_FOLDER
#define MHTTP_SVG_FOLDER MCONFIG_HTTP_SVG_FOLDER
#define MHTTP_WEBP_FOLDER MCONFIG_HTTP_WEBP_FOLDER
#define MHTTP_XICON_FOLDER MCONFIG_HTTP_XICON_FOLDER
#define MHTTP_PDF_FOLDER MCONFIG_HTTP_PDF_FOLDER
#define MHTTP_ZIP_FOLDER MCONFIG_HTTP_ZIP_FOLDER

#define container_of(ptr, type, member) \
        ((type*)((unsigned char*)(ptr) - offsetof(type, member)))


typedef enum {
        MCONN_SSL_INIT,
        MCONN_READING,
        MCONN_WRITING,
} mconn_state_t;

typedef enum {
        MHTTP_RESP_WRITE_HEADER,
        MHTTP_RESP_WRITE_FILE,
        MHTTP_RESP_WRITE_BODY
} mhttp_response_state_t;

typedef enum {
        MHTTP_SUCCESS,
        MHTTP_TOO_LARGE,
        MHTTP_BAD_REQ,
        MHTTP_VER_NOT_SUPPORTED,
        MHTTP_INTERNAL_SERVER_ERR,
        MHTTP_NOT_FOUND,
        MHTTP_METHOD_NOT_ALLOWED,
        MHTTP_NOT_IMPLEMENTED
} mhttp_status_t;

typedef enum {
        MCONN_KEEP_ALIVE,
        MCONN_CLOSE
} mconn_connect_t;

typedef enum {
        MHTTP_METHOD_GET,
        MHTTP_METHOD_HEAD,
        MHTTP_METHOD_POST,
        MHTTP_METHOD_PUT,
        MHTTP_METHOD_DELETE
} mhttp_method_t;

struct mlist_head {
        struct mlist_head *next;
        struct mlist_head *prev;
};

typedef struct mserv_hm_node {
        void *key;
        void *value;

        size_t ksize;
        void (*free_callback)(void *key, size_t ksize, void *val);
        struct mlist_head list;
} mserv_hm_node_t;

struct mserv_hashmap {
        mserv_hm_node_t **buckets;
        size_t capacity;
        size_t count;
};

struct mparser {
        char header[MHTTP_HEADER_SIZE];
        size_t elem_start;
        size_t header_len;
};

typedef struct {
        char *data;
        size_t len;
} mhttp_str_t;

typedef struct {
        void *bytes;
        void (*free_callback)(void *body);
} mhttp_body_t;

typedef struct {
        mhttp_str_t version;
        mhttp_str_t method;
        mhttp_str_t url;
        mhttp_str_t content_type;

        mhttp_body_t body;
        uint64_t content_len;

        mhttp_status_t status_code;
} mhttp_msg_t;

typedef struct {
        mhttp_status_t status_code;
        mhttp_body_t body;
        size_t content_len;
        char *content_type;
        int data_fd;
} mhttp_route_response_t;

typedef int (*mhttp_api_handler_t)(const mhttp_msg_t*, mhttp_route_response_t*, mhttp_method_t method);

struct mhttp_route {
        struct mserv_module *module;
        mhttp_api_handler_t handler;
};

struct mhttp_request {
        struct mparser parser_data;
        mhttp_msg_t msg;
        int parsed;
};

struct mserv_module {
        int (*route_register)(struct mserv_module *mod, const char *path, mhttp_api_handler_t handler);
        void *handle;
        char *url;
        char full_path[literal_len(MSERV_MODULES_FOLDER) + 255 + 2];
        struct mlist_head list;
};

struct mhttp_response {
        char header[MHTTP_HEADER_SIZE];
        size_t header_len;
        size_t header_pos;

        int data_fd;
        uint64_t offset; // for file writing and body
        size_t fsize;

        mhttp_msg_t msg;
        mhttp_status_t status_code;
        mhttp_response_state_t state;
};

struct mobject_node {
        struct mobject_node *next;
};

struct mobject_chunk {
        void *area;
        struct mlist_head list;
};

struct mobject_pool {
        size_t obj_size;
        size_t chunk_size;

        struct mobject_node *first_free;
        struct mobject_chunk *chunks_head;

        size_t total_alloc;
};

struct mconnection {
        int fd;
        struct mserver *serv;

        llhttp_t parser;

        mconn_state_t state;
        mconn_connect_t conn_type;

        struct mhttp_request *req;
        struct mhttp_response *res;
        
        SSL *ssl;
};

struct mserv_modules {
        struct mserv_module *modules_head;
        pthread_t watcher;
        pthread_mutex_t mutex;
};

struct mserver {
        int servfd;
        int epfd;

        llhttp_settings_t parser_settings;

        struct mconnection *conns;
        size_t conns_count;
        size_t conns_capacity;

        struct mobject_pool mhttp_request_pool;
        struct mobject_pool mhttp_response_pool;
        struct mobject_pool mserv_module_pool;

        struct sockaddr_in addr;
        socklen_t addrlen;

        unsigned long cached_time;
        char *project_path;
        struct mserv_hashmap mhttp_route_map;
        struct mserv_modules modules_data;

        const SSL_METHOD *ssl_method;
        SSL_CTX *ssl_ctx;
};

// server.c
int mserv_init(struct mserver *serv);
int mserv_event_loop(struct mserver *serv);
void mserv_destroy(struct mserver *serv);
void mserv_time_update(void);


// hashmap.c
int mserv_hm_init(struct mserv_hashmap *map, size_t capacity);
int mserv_hm_add(struct mserv_hashmap *map, void *key, size_t ksize, void *val, void (*free_callback)(void *key, size_t ksize, void *val));
void mserv_hm_node_free(mserv_hm_node_t *node);
void mserv_hm_delete(struct mserv_hashmap *map, void *key, size_t ksize);
void *mserv_hm_find(struct mserv_hashmap *map, void *key, size_t ksize);
void mserv_hm_destroy(struct mserv_hashmap *map);


// object_pool.c
int mobj_pool_init(struct mobject_pool *pool, size_t obj_size, size_t chunk_count);
void *mobj_pool_alloc(struct mobject_pool *pool);
void mobj_pool_free(struct mobject_pool *pool, void *ptr);
void mobj_pool_destroy(struct mobject_pool *pool);


// list.c
void mlist_head_init(struct mlist_head *head);
void mlist_add(struct mlist_head *head, struct mlist_head *el);
void mlist_del(struct mlist_head *el);


// module.c
void mserv_module_init(struct mserv_module *mod);
void mserv_module_destroy(struct mserv_module *mod);
int mserv_module_import(struct mserv_module *modules_head, const char *path);
void *mserv_module_watch_worker(void *arg);


// route.c
int mhttp_route_api_register(struct mserv_module *mod, mhttp_api_handler_t handler);
void mhttp_route_api_unregister(const char *url);


// connection.c
int mconn_init(struct mconnection *conn, struct mserver *serv, int fd);
int mconn_step(struct mconnection *conn);
int mconn_read(struct mconnection *conn);
int mconn_write(struct mconnection *conn);
void mconn_destroy(struct mconnection *conn);


// parser.c
int mparser_settings_init(llhttp_settings_t *http_sett);
void mparser_init(struct mparser *mprs);
void mparser_destroy(struct mparser *mprs);


// http.c
void mhttp_request_init(struct mhttp_request *req);
void mhttp_request_destroy(struct mhttp_request *req);
void mhttp_response_init(struct mhttp_response *res);
int mhttp_response_generate(struct mconnection *conn);
void mhttp_response_destroy(struct mhttp_response *res);


// utils.c
void fd_set_nonblock(int fd);