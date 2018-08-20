#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "zhttpd.h"
#include "zimg.h"
#include "zmd5.h"
#include "zutil.h"
#include "zlog.h"
#include "zdb.h"
#include "zaccess.h"
#include "cjson/cJSON.h"
#include "xhttpd.h"

typedef struct
{
    evhtp_request_t *req;
    thr_arg_t *thr_arg;
    char address[16];
    int partno;
    int succno;
    int check_name;
} mp_arg_t;

static const char *post_error_list[] = {
    "Internal error.",
    "File type not support.",
    "Request method error.",
    "Access error.",
    "Request body parse error.",
    "Content-Length error.",
    "Content-Type error.",
    "File too large.",
    "Request url illegal.",
    "Image not existed."};

static const char *method_strmap[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "DELETE",
    "MKCOL",
    "COPY",
    "MOVE",
    "OPTIONS",
    "PROPFIND",
    "PROPATCH",
    "LOCK",
    "UNLOCK",
    "TRACE",
    "CONNECT",
    "PATCH",
    "UNKNOWN",
};


void base64_request_cb(evhtp_request_t *req, void *arg);
static evthr_t *get_request_thr(evhtp_request_t *request);



/**
 * @brief get_request_thr get the request's thread
 *
 * @param request the evhtp request
 *
 * @return the thread dealing with the request
 */
static evthr_t *get_request_thr(evhtp_request_t *request)
{
    evhtp_connection_t *htpconn;
    evthr_t *thread;

    htpconn = evhtp_request_get_connection(request);
    thread = htpconn->thread;

    return thread;
}
void base64_request_cb(evhtp_request_t *req, void *arg)
{
    char *buff = NULL;
    char *temp = NULL;
    int err_no = 0;
    int ret_json = 1;
    evhtp_connection_t *ev_conn = evhtp_request_get_connection(req);
    struct sockaddr *saddr = ev_conn->saddr;
    struct sockaddr_in *ss = (struct sockaddr_in *)saddr;
    char address[16];

    const char *xff_address = evhtp_header_find(req->headers_in, "X-Forwarded-For");
    if (xff_address)
    {
        inet_aton(xff_address, &ss->sin_addr);
    }
    strncpy(address, inet_ntoa(ss->sin_addr), 16);

    int req_method = evhtp_request_get_method(req);
    if (req_method >= 16)
        req_method = 16;
    LOG_PRINT(LOG_DEBUG, "Method: %d", req_method);
    if (strcmp(method_strmap[req_method], "POST") != 0)
    {
        LOG_PRINT(LOG_DEBUG, "Request Method Not Support.");
        LOG_PRINT(LOG_INFO, "%s refuse post method", address);
        err_no = 2;
        goto err;
    }
    evhtp_kvs_t *params;
    params = req->uri->query;
    if (params == NULL)
    {
        LOG_PRINT(LOG_DEBUG, "no data upload");
        goto err;
    }
    const char *base64 = evhtp_kv_find(params, "base64");
    if (!base64)
    {
        LOG_PRINT(LOG_DEBUG, "no data upload");
        goto err;
    }

    LOG_PRINT(LOG_DEBUG, "before url decode");
    temp = malloc(strlen(base64)+1);
    int len = url_decode(base64,temp);
    LOG_PRINT(LOG_DEBUG, "end url decode");

    int str_len = len / 4 * 3 - 2;
    //判断编码后的字符串后是否有=
    // if (strstr(temp, "=="))
    //     str_len = len / 4 * 3 - 2;
    // else if (strstr(temp, "="))
    //     str_len = len / 4 * 3 - 1;
    // else
    //     str_len = len / 4;

    buff = malloc(str_len);
    if (!buff)
    {
        LOG_PRINT(LOG_DEBUG, "temp buff Malloc Failed!");
        goto err;
    }

    LOG_PRINT(LOG_DEBUG, "before b64_decode");
    int length = b64_decode(temp, len, buff);
    LOG_PRINT(LOG_DEBUG, "end b64_decode");
    evthr_t *thread = get_request_thr(req);
    thr_arg_t *thr_arg = (thr_arg_t *)evthr_get_aux(thread);
    char md5sum[33];
    if (save_img(thr_arg, buff, length, md5sum) == -1)
    {
        LOG_PRINT(LOG_DEBUG, "Image Save Failed!");
        LOG_PRINT(LOG_ERROR, "%s fail post save", address);
        goto err;
    }
    err_no = -1;
    LOG_PRINT(LOG_INFO, "%s succ post pic:%s size:%d", address, md5sum, length);
    json_return(req, err_no, md5sum, length);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============base64_request_cb() SUCC!===============");
    goto done;
err:
    if (temp != NULL)
    {
        free(temp);
    }
    if (buff != NULL)
    {
        free(buff);
    }
    if (ret_json == 0)
    {
        evbuffer_add_printf(req->buffer_out, "<h1>Upload Failed!</h1></body></html>");
        evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    }
    else
    {
        json_return(req, err_no, NULL, 0);
    }
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============base64_request_cb() ERROR!===============");
done:
    free(temp);
    free(buff);
}