#include <mserv.h>
#include <string.h>

static int test_handler(const mhttp_msg_t *req,
                        mhttp_route_response_t *res, mhttp_method_t method)
{
        (void)req;
        (void)method;

        if (method == MHTTP_METHOD_GET || method == MHTTP_METHOD_HEAD) {
                char *body = "{\"status\":\"da\"}";

                res->status_code = MHTTP_SUCCESS;
                res->body.bytes = (method == MHTTP_METHOD_HEAD) ? NULL : body;
                res->content_len = strlen(body);
                res->content_type = "application/json";
        }
        else {
                res->status_code = MHTTP_METHOD_NOT_ALLOWED;
        }

        return 0;
}

MSERV_DEFINE_API_MODULE("/test", test_handler)