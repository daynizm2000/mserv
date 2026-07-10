#include <mserv.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h> // IWYU pragma: keep

#define MHTTP_VER_1_0_FMT "1.0"
#define MHTTP_VER_1_1_FMT "1.1"

#define MHTTP_METHOD_GET_STR "GET"
#define MHTTP_METHOD_HEAD_STR "HEAD"
#define MHTTP_METHOD_POST_STR "POST"
#define MHTTP_METHOD_PUT_STR "PUT"
#define MHTTP_METHOD_DELETE_STR "DELETE"

#define __mhttp_validate_cmp(a, b, alen) ((alen) == sizeof((b)) - 1 && memcmp((a), (b), (alen)) == 0)
#define __mhttp_suffix_validate(a, b, alen) ((alen) > sizeof((b)) - 1 && memcmp((a) + ((alen) - (sizeof((b)) - 1)), (b), sizeof((b)) - 1) == 0)

static const struct {
        mhttp_status_t code; // code = index
        const char *msg;
} mhttp_status_code_fmts[] = {
        {MHTTP_SUCCESS, "200 OK"},
        {MHTTP_TOO_LARGE, "414 Request-URI Too Long"},
        {MHTTP_BAD_REQ, "400 Bad Request"},
        {MHTTP_VER_NOT_SUPPORTED, "505 HTTP Version Not Supported"},
        {MHTTP_INTERNAL_SERVER_ERR, "500 Internal Server Error"},
        {MHTTP_NOT_FOUND, "404 Not Found"},
        {MHTTP_METHOD_NOT_ALLOWED, "405 Method Not Allowed"},
        {MHTTP_NOT_IMPLEMENTED, "501 Not Implemented"}
};

static const struct {
        mhttp_method_t method; // method = index
        mhttp_str_t name;
} mhttp_method_fmts[] = {
        {MHTTP_METHOD_GET, {MHTTP_METHOD_GET_STR, literal_len(MHTTP_METHOD_GET_STR)}},
        {MHTTP_METHOD_HEAD, {MHTTP_METHOD_HEAD_STR, literal_len(MHTTP_METHOD_HEAD_STR)}},
        {MHTTP_METHOD_POST, {MHTTP_METHOD_POST_STR, literal_len(MHTTP_METHOD_POST_STR)}},
        {MHTTP_METHOD_PUT, {MHTTP_METHOD_PUT_STR, literal_len(MHTTP_METHOD_PUT_STR)}},
        {MHTTP_METHOD_DELETE, {MHTTP_METHOD_DELETE_STR, literal_len(MHTTP_METHOD_DELETE_STR)}}
};

static const struct __mhttp_mime {
        const mhttp_str_t ext;
        const mhttp_str_t mime;
        const mhttp_str_t folder;
} mhttp_mime_types[] = {
        {{".html", literal_len(".html")}, {MHTTP_CONT_TYPE_HTML, literal_len(MHTTP_CONT_TYPE_HTML)},  {MHTTP_HTML_FOLDER, literal_len(MHTTP_HTML_FOLDER)}},
        {{".css",  literal_len(".css")},  {MHTTP_CONT_TYPE_CSS,  literal_len(MHTTP_CONT_TYPE_CSS)},   {MHTTP_CSS_FOLDER,  literal_len(MHTTP_CSS_FOLDER)}},
        {{".js",   literal_len(".js")},   {MHTTP_CONT_TYPE_JS,   literal_len(MHTTP_CONT_TYPE_JS)},    {MHTTP_JS_FOLDER,   literal_len(MHTTP_JS_FOLDER)}},
        {{".txt",  literal_len(".txt")},  {MHTTP_CONT_TYPE_TEXT, literal_len(MHTTP_CONT_TYPE_TEXT)},  {MHTTP_TXT_FOLDER,  literal_len(MHTTP_TXT_FOLDER)}},
        {{".json", literal_len(".json")}, {MHTTP_CONT_TYPE_JSON, literal_len(MHTTP_CONT_TYPE_JSON)},  {MHTTP_JSON_FOLDER, literal_len(MHTTP_JSON_FOLDER)}},
        {{".xml",  literal_len(".xml")},  {MHTTP_CONT_TYPE_XML,  literal_len(MHTTP_CONT_TYPE_XML)},   {MHTTP_XML_FOLDER,  literal_len(MHTTP_XML_FOLDER)}},
        {{".jpeg", literal_len(".jpeg")}, {MHTTP_CONT_TYPE_JPEG, literal_len(MHTTP_CONT_TYPE_JPEG)},  {MHTTP_JPEG_FOLDER, literal_len(MHTTP_JPEG_FOLDER)}},
        {{".jpg",  literal_len(".jpg")},  {MHTTP_CONT_TYPE_JPEG, literal_len(MHTTP_CONT_TYPE_JPEG)},  {MHTTP_JPEG_FOLDER, literal_len(MHTTP_JPEG_FOLDER)}},
        {{".png",  literal_len(".png")},  {MHTTP_CONT_TYPE_PNG,  literal_len(MHTTP_CONT_TYPE_PNG)},   {MHTTP_PNG_FOLDER,  literal_len(MHTTP_PNG_FOLDER)}},
        {{".gif",  literal_len(".gif")},  {MHTTP_CONT_TYPE_GIF,  literal_len(MHTTP_CONT_TYPE_GIF)},   {MHTTP_GIF_FOLDER,  literal_len(MHTTP_GIF_FOLDER)}},
        {{".svg",  literal_len(".svg")},  {MHTTP_CONT_TYPE_SVG,  literal_len(MHTTP_CONT_TYPE_SVG)},   {MHTTP_SVG_FOLDER,  literal_len(MHTTP_SVG_FOLDER)}},
        {{".webp", literal_len(".webp")}, {MHTTP_CONT_TYPE_WEBP, literal_len(MHTTP_CONT_TYPE_WEBP)},  {MHTTP_WEBP_FOLDER, literal_len(MHTTP_WEBP_FOLDER)}},
        {{".ico",  literal_len(".ico")},  {MHTTP_CONT_TYPE_XICON, literal_len(MHTTP_CONT_TYPE_XICON)},{MHTTP_XICON_FOLDER, literal_len(MHTTP_XICON_FOLDER)}},
        {{".pdf",  literal_len(".pdf")},  {MHTTP_CONT_TYPE_PDF,  literal_len(MHTTP_CONT_TYPE_PDF)},   {MHTTP_PDF_FOLDER,  literal_len(MHTTP_PDF_FOLDER)}},
        {{".zip",  literal_len(".zip")},  {MHTTP_CONT_TYPE_ZIP,  literal_len(MHTTP_CONT_TYPE_ZIP)},   {MHTTP_ZIP_FOLDER,  literal_len(MHTTP_ZIP_FOLDER)}},
};


void mhttp_request_init(struct mhttp_request *req)
{
        memset(req, 0, sizeof(struct mhttp_request));
        req->msg.status_code = MHTTP_SUCCESS;
        mparser_init(&req->parser_data);
}

void mhttp_request_destroy(struct mhttp_request *req)
{
        if (req->msg.body.free_callback)
                req->msg.body.free_callback(req->msg.body.bytes);

        mparser_destroy(&req->parser_data);
        memset(req, 0, sizeof(struct mhttp_request));
        req->msg.status_code = MHTTP_SUCCESS;
}

void mhttp_response_init(struct mhttp_response *res)
{
        if (!res)
                return;

        memset(res, 0, sizeof(struct mhttp_response));
        res->data_fd = -1;
        res->state = MHTTP_RESP_WRITE_HEADER;
}

static mhttp_status_t mhttp_msg_version_validate(mhttp_msg_t *msg)
{
        mhttp_str_t *ver = &msg->version;
        
        if (!__mhttp_validate_cmp(ver->data, MHTTP_VER_1_0_FMT, ver->len) &&
                !__mhttp_validate_cmp(ver->data, MHTTP_VER_1_1_FMT, ver->len))
                        mserv_warn_ret(MHTTP_VER_NOT_SUPPORTED, "Request validate error. Invalid HTTP version\n");
        
        return MHTTP_SUCCESS;
}

static void mhttp_str_response_generate(struct mconnection *conn)
{
        struct mhttp_response *res = conn->res;
        mhttp_msg_t *msg = &res->msg;
        const char *code_msg = mhttp_status_code_fmts[msg->status_code].msg;

        res->header_len = snprintf(res->header, MHTTP_HEADER_SIZE - 1,
                                "HTTP/%.*s %s\r\n"
                                "Server: %s/%s\r\n"
                                "Content-Type: %.*s\r\n"
                                "Content-Length: %zu\r\n"
                                "Connection: %s\r\n"
                                "\r\n"
                                "%s",
                        (msg->status_code != MHTTP_VER_NOT_SUPPORTED) ? (int)msg->version.len : (int)(literal_len(MHTTP_VER_1_1_FMT)),   
                        (msg->status_code != MHTTP_VER_NOT_SUPPORTED) ? msg->version.data : MHTTP_VER_1_1_FMT,
                        code_msg, MSERV_NAME, MSERV_VERSION, (int)msg->content_type.len, msg->content_type.data, msg->content_len,
                        (msg->status_code != MHTTP_SUCCESS) ? "close" : (conn->conn_type == MCONN_KEEP_ALIVE) ? "keep-alive" : "close",
                        (msg->status_code != MHTTP_SUCCESS) ? code_msg : "");
}

static void mhttp_err_response_generate(struct mconnection *conn, mhttp_status_t code)
{
        struct mhttp_response *res = conn->res;
        struct mhttp_request *req = conn->req;
        mhttp_msg_t *msg = &res->msg;

        msg->version.data = (req->msg.version.len) ? req->msg.version.data : MHTTP_VER_1_1_FMT;
        msg->version.len = (req->msg.version.len) ? req->msg.version.len : literal_len(MHTTP_VER_1_1_FMT);
        msg->status_code = code;
        msg->content_type.data = MHTTP_CONT_TYPE_TEXT;
        msg->content_type.len = literal_len(MHTTP_CONT_TYPE_TEXT);
        msg->content_len = strlen(mhttp_status_code_fmts[msg->status_code].msg);

        mhttp_str_response_generate(conn);
}

static mhttp_status_t mhttp_full_path_generate(mhttp_msg_t *msg, char **dest)
{
        char *full_path;

        full_path = malloc(literal_len("/static/") + msg->url.len + strlen(g_mserv_obj.project_path) + 1);
        if (!full_path)
                mserv_err_ret(MHTTP_INTERNAL_SERVER_ERR, "Memory allocation error\n");

        strcpy(full_path, g_mserv_obj.project_path);
        strcat(full_path, "/static/");
        strncat(full_path, msg->url.data, msg->url.len);

        *dest = full_path;

        return MHTTP_SUCCESS;
}

mhttp_status_t mhttp_api_handle(struct mconnection *conn, mhttp_method_t method)
{
        struct mhttp_request *req = conn->req;
        mhttp_msg_t *msg = &req->msg;
        struct mhttp_response *res = conn->res;
        struct mhttp_route *api;
        mhttp_route_response_t resp = {0};
        
        resp.data_fd = -1;
        
        pthread_mutex_lock(&g_mserv_obj.modules_data.mutex);

        api = mserv_hm_find(&g_mserv_obj.mhttp_route_map, msg->url.data + literal_len("/api"), msg->url.len - literal_len("/api"));
        if (!api) {
                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);
                return MHTTP_NOT_FOUND;
        }

        if (api->handler(msg, &resp, method) < 0) {
                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);
                
                mserv_err_ret(MHTTP_INTERNAL_SERVER_ERR, "API Callback error\n");
        }
        else {
                pthread_mutex_unlock(&g_mserv_obj.modules_data.mutex);

                if (resp.status_code != MHTTP_SUCCESS) {
                        return resp.status_code;
                }
                else {
                        if (resp.data_fd >= 0) {
                                if (!resp.content_len) {
                                        struct stat st;

                                        if (fstat(resp.data_fd, &st) != 0)
                                                mserv_warn_ret(MHTTP_INTERNAL_SERVER_ERR, "Error opening file statistics: %s\n", strerror(errno));

                                        res->msg.content_len = st.st_size;
                                }
                                else {
                                        res->msg.content_len = resp.content_len;
                                }

                                if (method != MHTTP_METHOD_HEAD)
                                        res->data_fd = resp.data_fd;

                                res->fsize = res->msg.content_len;
                        }
                        else {
                                if (method != MHTTP_METHOD_HEAD)
                                        res->msg.body = resp.body;

                                res->msg.content_len = resp.content_len;
                        }

                        res->msg.version = req->msg.version;
                        res->msg.content_type.data = resp.content_type;
                        res->msg.content_type.len = strlen(resp.content_type);
                        res->msg.status_code = resp.status_code;

                        mhttp_str_response_generate(conn);
                }
        }
        
        return MHTTP_SUCCESS;
}

mhttp_status_t mhttp_url_validate(mhttp_str_t *url)
{
        if (__mhttp_validate_cmp(url->data, "/", url->len)) {
                url->data = MHTTP_MAIN_HTML_PAGE_PATH;
                url->len = literal_len(MHTTP_MAIN_HTML_PAGE_PATH);
        }

        for (size_t i = 0; i < sizeof(mhttp_mime_types) / sizeof(*mhttp_mime_types); i++) {
                const struct __mhttp_mime *mtype = &mhttp_mime_types[i];

                if (url->len >= mtype->ext.len && memcmp(url->data + (url->len - mtype->ext.len), mtype->ext.data, mtype->ext.len) == 0) {
                        if (url->len > mtype->folder.len && memcmp(url->data, mtype->folder.data, mtype->folder.len) == 0) {
                                return MHTTP_SUCCESS;
                        }
                        else {
                                if (__mhttp_validate_cmp(mtype->mime.data, MHTTP_CONT_TYPE_JSON, mtype->mime.len) ||
                                        __mhttp_validate_cmp(mtype->mime.data, MHTTP_CONT_TYPE_XML, mtype->mime.len)) {
                                                return MHTTP_NOT_FOUND;
                                }
                                else {
                                        if (memchr(url->data + 1, '/', url->len - 1)) {
                                                return MHTTP_NOT_FOUND;
                                        }

                                        return MHTTP_SUCCESS;
                                }
                        }
                }
        }

        return MHTTP_NOT_FOUND;
}

int mhttp_find_content_type(mhttp_str_t *url, mhttp_str_t *dest)
{
        for (size_t i = 0; i < sizeof(mhttp_mime_types) / sizeof(*mhttp_mime_types); i++) {
                const struct __mhttp_mime *mtype = &mhttp_mime_types[i];

                if (url->len >= mtype->ext.len && memcmp(url->data + (url->len - mtype->ext.len), mtype->ext.data, mtype->ext.len) == 0) {
                        *dest = mtype->mime;
                        return 1;
                }
        }

        return 0;
}

mhttp_status_t mhttp_static_file_handle(struct mconnection *conn, mhttp_method_t method)
{
        struct mhttp_request *req = conn->req;
        struct mhttp_response *res = conn->res;
        mhttp_msg_t *req_msg = &req->msg;
        struct stat st;
        char *full_path;
        mhttp_status_t ret;
        mhttp_str_t cont_type = {0};

        if (method != MHTTP_METHOD_GET && method != MHTTP_METHOD_HEAD)
                mserv_warn_ret(MHTTP_METHOD_NOT_ALLOWED, "Method is not supported for static files\n");

        if ((ret = mhttp_url_validate(&req->msg.url)) != MHTTP_SUCCESS)
                mserv_warn_ret(ret, "Response generate error. Invalid url\n");

        if ((ret = mhttp_full_path_generate(req_msg, &full_path)) != MHTTP_SUCCESS)
                return ret;

        if (method == MHTTP_METHOD_HEAD) {
                res->data_fd = -1;

                if (lstat(full_path, &st) != 0) {
                        mserv_warn_ret((errno == ENOENT) ? MHTTP_NOT_FOUND : MHTTP_INTERNAL_SERVER_ERR,
                                "Error opening file statistics: %s\n", strerror(errno));
                                
                        free(full_path);
                }
        }
        else if (method == MHTTP_METHOD_GET) {
                res->data_fd = open(full_path, O_RDONLY);

                if (res->data_fd < 0) {
                        mserv_warn_ret((errno == ENOENT) ? MHTTP_NOT_FOUND : MHTTP_INTERNAL_SERVER_ERR,
                                "Error opening file: %s\n", strerror(errno));

                        free(full_path);
                }

                if (fstat(res->data_fd, &st) != 0) {
                        mserv_warn_ret(MHTTP_INTERNAL_SERVER_ERR, "Error opening file statistics: %s\n", strerror(errno));
                        free(full_path);
                }
        }

        res->fsize = st.st_size;
        free(full_path);

        mhttp_find_content_type(&req->msg.url, &cont_type);

        res->msg.version = req->msg.version;
        res->msg.url = req->msg.url;
        res->msg.status_code = MHTTP_SUCCESS;
        res->msg.content_len = res->fsize;
        res->msg.content_type = cont_type;

        mhttp_str_response_generate(conn);

        return MHTTP_SUCCESS;
}

static mhttp_status_t mhttp_find_method(mhttp_str_t *name, mhttp_method_t *dest)
{
        for (size_t i = 0; i < sizeof(mhttp_method_fmts) / sizeof(*mhttp_method_fmts); i++) {
                const mhttp_str_t *elem = &mhttp_method_fmts[i].name;

                if (name->len == elem->len && memcmp(name->data, elem->data, name->len) == 0) {
                        *dest = mhttp_method_fmts[i].method;
                        return MHTTP_SUCCESS;
                }
        }

        return MHTTP_NOT_IMPLEMENTED;
}

int mhttp_response_generate(struct mconnection *conn)
{
        mhttp_status_t ret;
        mhttp_msg_t *msg = &conn->req->msg;
        mhttp_method_t method;

        if (msg->status_code != MHTTP_SUCCESS) {
                mhttp_err_response_generate(conn, msg->status_code);
                return 0;
        }
        if ((ret = mhttp_find_method(&msg->method, &method)) != MHTTP_SUCCESS) {
                mhttp_err_response_generate(conn, ret);
                return 0;
        }
        if ((ret = mhttp_msg_version_validate(msg)) != MHTTP_SUCCESS) {
                mhttp_err_response_generate(conn, ret);
                return 0;
        }

        if (msg->url.len > literal_len("/api/") && memcmp(msg->url.data, "/api/", literal_len("/api/")) == 0) {
                if ((ret = mhttp_api_handle(conn, method)) != MHTTP_SUCCESS) {
                        mhttp_err_response_generate(conn, ret);
                }
        }
        else {
                if ((ret = mhttp_static_file_handle(conn, method)) != MHTTP_SUCCESS) {
                        mhttp_err_response_generate(conn, ret);
                }
        }

        return 0;
}

void mhttp_response_destroy(struct mhttp_response *res)
{
        if (!res)
                return;
        
        if (res->data_fd >= 0) {
                close(res->data_fd);
                res->data_fd = -1;
        }
        if (res->msg.body.free_callback)
                res->msg.body.free_callback(res->msg.body.bytes);

        res->header_len = 0;
        res->header_pos = 0;
        res->offset = 0;
        res->state = MHTTP_RESP_WRITE_HEADER;
}