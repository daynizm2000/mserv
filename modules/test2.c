#include "../include/mserv.h"
#include <fcntl.h>

static int test_handler(const mhttp_msg_t *req,
                        mhttp_route_response_t *res, mhttp_method_t method)
{
        (void)req;
        (void)method;

        res->status_code = MHTTP_SUCCESS;
        res->content_type = MHTTP_CONT_TYPE_HTML;
        res->data_fd = open("test/static/index.html", O_RDONLY);

        return 0;
}

MSERV_DEFINE_API_MODULE("/test2", test_handler)