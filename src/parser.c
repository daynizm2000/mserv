#include <mserv.h>
#include <llhttp.h>
#include <string.h>
#include <stdlib.h>

#define mparser_extract_line(parser, field) ({ \
                struct mhttp_request *________req = ((struct mconnection*)((parser)->data))->req; \
                struct mparser *________mprs = &________req->parser_data; \
                mhttp_msg_t *________msg = &________req->msg; \
                ________msg->field.data = ________mprs->header + ________mprs->elem_start; \
                ________msg->field.len = ________mprs->header_len - ________mprs->elem_start; \
                ________mprs->elem_start = ________mprs->header_len; \
        })

static int mparser_on_line(llhttp_t *parser, const char *data, unsigned long len)
{
        struct mhttp_request *req = ((struct mconnection*)parser->data)->req;
        struct mparser *mprs = &req->parser_data;

        if (len > MHTTP_HEADER_SIZE - mprs->header_len) {
                req->msg.status_code = MHTTP_TOO_LARGE;
                req->parsed = 1;
                return HPE_USER;
        }

        memcpy(mprs->header + mprs->header_len, data, len);
        mprs->header_len += len;

        return 0;
}

static int mparser_on_version_complete(llhttp_t *parser)
{
        mparser_extract_line(parser, version);
        return 0;
}

static int mparser_on_method_complete(llhttp_t *parser)
{
        mparser_extract_line(parser, method);
        return 0;
}

static int mparser_on_url_complete(llhttp_t *parser)
{
        mparser_extract_line(parser, url);
        return 0;
}

static int mparser_on_body(llhttp_t *parser, const char *data, unsigned long len)
{
        struct mhttp_request *req = ((struct mconnection*)parser->data)->req;

        if (!req->msg.body.bytes) {
                if (!parser->content_length)
                        return 0;

                req->msg.body.bytes = malloc(parser->content_length);
                if (!req->msg.body.bytes)
                        mserv_err_ret(HPE_USER, "Memory allocation error\n");

                req->msg.body.free_callback = free;
        }

        memcpy((unsigned char*)req->msg.body.bytes + req->msg.content_len, data, len);
        req->msg.content_len += len;

        return 0;
}

static int mparser_complete(llhttp_t *parser)
{
        struct mconnection *conn = parser->data;

        if (llhttp_should_keep_alive(parser))
                conn->conn_type = MCONN_KEEP_ALIVE;

        conn->req->parsed = 1;

        return 0;
}

int mparser_settings_init(llhttp_settings_t *http_sett)
{
        if (!http_sett)
                return -1;

        llhttp_settings_init(http_sett);
        http_sett->on_version = mparser_on_line;
        http_sett->on_version_complete = mparser_on_version_complete;
        http_sett->on_message_complete = mparser_complete;
        http_sett->on_method = mparser_on_line;
        http_sett->on_method_complete = mparser_on_method_complete;
        http_sett->on_url = mparser_on_line;
        http_sett->on_url_complete = mparser_on_url_complete;
        http_sett->on_body = mparser_on_body;

        return 0;
}

void mparser_init(struct mparser *mprs)
{
        if (!mprs)
                return;

        mprs->elem_start = 0;
        mprs->header_len = 0;
}

void mparser_destroy(struct mparser *mprs)
{
        if (!mprs)
                return;

        mprs->elem_start = 0;
        mprs->header_len = 0;
}