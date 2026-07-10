#include <mserv.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <unistd.h>
#include <string.h>
#include <sys/sendfile.h>

static int mconn_ssl_init(struct mconnection *conn)
{
        int ret;

        if (!conn->ssl) {
                conn->ssl = SSL_new(g_mserv_obj.ssl_ctx);
                if (!conn->ssl)
                        mserv_warn_ret(-1, "Error creating TLS object for client\n");

                if (SSL_set_fd(conn->ssl, conn->fd) != 1)
                        mserv_warn_ret(-1, "Failed to bind the client socket to SSL\n");
        }
        
        if ((ret = SSL_accept(conn->ssl)) <= 0) {
                int n = SSL_get_error(conn->ssl, ret);

                if (n == SSL_ERROR_WANT_WRITE || n == SSL_ERROR_WANT_READ) {
                        conn->state = MCONN_SSL_INIT;
                        return 0;
                }

                mserv_warn_ret(-1, "TLS Handshake failure\n");
        }

        if (!BIO_get_ktls_send(SSL_get_wbio(conn->ssl)))
                mserv_err_ret(-1, "kLTS failure\n");

        conn->state = MCONN_READING;
        return 0;
}

static void mconn_ssl_destroy(struct mconnection *conn)
{
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
}

int mconn_init(struct mconnection *conn, struct mserver *serv, int fd)
{
        if (!conn)
                return -1;
        
        memset(conn, 0, sizeof(struct mconnection));

        conn->req = mobj_pool_alloc(&serv->mhttp_request_pool);
        if (!conn->req)
                mserv_warn_ret(-1, "Failed to allocate the request object in the pool\n");

        conn->res = mobj_pool_alloc(&serv->mhttp_response_pool);
        if (!conn->res)
                mserv_warn_goto(fail, "Failed to allocate the response object in the pool\n");

        mhttp_response_init(conn->res);
        mhttp_request_init(conn->req);

        conn->serv = serv;
        conn->conn_type = MCONN_CLOSE;
        conn->fd = fd;
        llhttp_init(&conn->parser, HTTP_REQUEST, &serv->parser_settings);
        conn->parser.data = conn;
        conn->state = MCONN_READING;

        if (mconn_ssl_init(conn) < 0)
                goto fail;
        
        return 0;
fail:
        if (conn->req)
                mobj_pool_free(&serv->mhttp_request_pool, conn->req);
        if (conn->res)
                mobj_pool_free(&serv->mhttp_response_pool, conn->res);

        return -1;
}

int mconn_step(struct mconnection *conn)
{
        switch (conn->state) {
                case MCONN_SSL_INIT: {
                        return mconn_ssl_init(conn);
                }
                case MCONN_READING: {
                        return mconn_read(conn);
                }
                case MCONN_WRITING: {
                        return mconn_write(conn);
                }
                default: {
                        return -1;
                }
        }
}

int mconn_read(struct mconnection *conn)
{
        ssize_t read_bytes;
        char buffer[4096];

        while (!conn->req->parsed) {
                mserv_log("Read client %d...\n", conn->fd);
                read_bytes = SSL_read(conn->ssl, buffer, sizeof(buffer));

                if (read_bytes < 0) {
                        int err = SSL_get_error(conn->ssl, read_bytes);

                        if (err == SSL_ERROR_WANT_READ)
                                return 0;

                        mserv_warn_ret(-1, "Client socket read error\n");
                } 
                else if (!read_bytes) {
                        mserv_log_ret(-1, "The client socket closed the connection\n");
                }

                if (llhttp_execute(&conn->parser, buffer, read_bytes) != HPE_OK) {
                        mserv_warn("Data parsing error\n");

                        if (conn->req->msg.status_code != MHTTP_SUCCESS) {
                                mserv_log("Started generating response...\n");

                                if (mhttp_response_generate(conn) < 0)
                                        mserv_warn_ret(-1, "Error generating response\n");

                                break;
                        }

                        llhttp_reset(&conn->parser);
                        return -1;
                }
        }

        if (conn->req->parsed) {
                mserv_log("Started generating response...\n");

                if (mhttp_response_generate(conn) < 0)
                        mserv_warn_ret(-1, "Error generating response\n");

                mhttp_request_destroy(conn->req);
        }

        return mconn_write(conn);
}

int mconn_write(struct mconnection *conn)
{
        if (conn->res->state == MHTTP_RESP_WRITE_HEADER) {
                while (conn->res->header_pos < conn->res->header_len) {
                        mserv_log("Sending an HTTP header to the client socket...\n");

                        ssize_t written = SSL_write(conn->ssl,
                                conn->res->header + conn->res->header_pos,
                                conn->res->header_len - conn->res->header_pos);

                        if (written < 0) {
                                int err = SSL_get_error(conn->ssl, written);

                                if (err == SSL_ERROR_WANT_WRITE)
                                        return 0;

                                mserv_warn_goto(fail, "Error writing data to the client socket\n");
                        }
                        else if (!written)
                                mserv_log_goto(fail, "The client socket closed the connection\n");

                        conn->res->header_pos += written;
                }

                conn->res->state = (conn->res->data_fd >= 0) ? MHTTP_RESP_WRITE_FILE : MHTTP_RESP_WRITE_BODY;
        }
        if (conn->res->state == MHTTP_RESP_WRITE_FILE) {
                mserv_log("Sending file data to a client socket...\n");

                while (conn->res->offset < conn->res->fsize) {
                        ssize_t written = sendfile(conn->fd, conn->res->data_fd,
                                (off_t*)&conn->res->offset, conn->res->fsize - conn->res->offset);
                        
                        if (written < 0) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                        return 0;

                                mserv_warn_goto(fail, "Error writing data to the client socket\n");
                        }
                        else if (!written) {
                                mserv_log_goto(fail, "The client socket closed the connection\n");
                        }
                }
                
                goto exit;
        }
        if (conn->res->state == MHTTP_RESP_WRITE_BODY) {
                void *data = conn->res->msg.body.bytes;

                if (!data || !conn->res->msg.content_len)
                        goto exit;

                mserv_log("Sending an body to the client socket...\n");

                while (conn->res->offset < conn->res->msg.content_len) {
                        ssize_t written = SSL_write(conn->ssl, (unsigned char*)data + conn->res->offset,
                                conn->res->msg.content_len - conn->res->offset);
                        
                        if (written < 0) {
                                int err = SSL_get_error(conn->ssl, written);

                                if (err == SSL_ERROR_WANT_WRITE)
                                        return 0;

                                mserv_warn_goto(fail, "Error SSL writing data to the client socket\n");
                        }
                        else if (!written) {
                                mserv_log_goto(fail, "The client socket closed the connection\n");
                        }

                        conn->res->offset += written;
                }

                goto exit;
        }

        return 0;
exit:
        llhttp_reset(&conn->parser);
        mhttp_response_destroy(conn->res);
        conn->state = MCONN_READING;
        return (conn->conn_type == MCONN_KEEP_ALIVE) ? 0 : -1; // -1 for closing connection (not error)
fail:
        llhttp_reset(&conn->parser);
        mhttp_response_destroy(conn->res);
        return -1; // error
}

void mconn_destroy(struct mconnection *conn)
{
        mconn_ssl_destroy(conn);

        if (conn->fd >= 0)
                close(conn->fd);

        llhttp_reset(&conn->parser);
        
        mobj_pool_free(&conn->serv->mhttp_request_pool, conn->req);
        mobj_pool_free(&conn->serv->mhttp_response_pool, conn->res);

        memset(conn, 0, sizeof(struct mconnection));

        conn->fd = -1;
        conn->state = MCONN_READING;
}