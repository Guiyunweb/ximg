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
#include "xhttpd.h"
#include "xlist.h"
#include "xutil.h"

typedef struct
{
    evhtp_request_t *req;
    thr_arg_t *thr_arg;
    char address[16];
    int partno;
    int succno;
    int check_name;
} mp_arg_t;
/**
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
**/
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
void displayInfo(img_info *info)
{
    printf("code:%d\tmsg:%s\tmd5:%s\n", info->code, info->msg, info->md5);
}
void base64_request_cb(evhtp_request_t *req, void *arg);
static evthr_t *get_request_thr(evhtp_request_t *request);
int x_binary_parse(evhtp_request_t *req, const char *content_type, const char *address, const char *buff, int post_size);
int x_multipart_parse(evhtp_request_t *req, const char *content_type, const char *address, const char *buff, int post_size);
void post_upload_request_cb(evhtp_request_t *req, void *arg);

int x_on_header_field(x_multipart_parser *p, const char *at, size_t length);
int x_on_header_value(x_multipart_parser *p, const char *at, size_t length);
int x_on_chunk_data(x_multipart_parser *p, const char *at, size_t length);

int x_on_header_field(x_multipart_parser *p, const char *at, size_t length)
{
    LOG_PRINT(LOG_DEBUG, "x_on_header_field excute");
    char *header_name = (char *)malloc(length + 1);
    snprintf(header_name, length + 1, "%s", at);
    LOG_PRINT(LOG_DEBUG, "x_on_header_field header_name %d %s: ", length, header_name);
    free(header_name);
    return 0;
}

int x_on_header_value(x_multipart_parser *p, const char *at, size_t length)
{
    LOG_PRINT(LOG_DEBUG, "x_on_header_value excute");
    mp_arg_t *mp_arg = (mp_arg_t *)x_multipart_parser_get_data(p);
    char *filename = strnstr(at, "filename=", length);
    char *nameend = NULL;
    if (filename)
    {
        filename += 9;
        if (filename[0] == '\"')
        {
            filename++;
            nameend = strnchr(filename, '\"', length - (filename - at));
            if (!nameend)
                mp_arg->check_name = -1;
            else
            {
                nameend[0] = '\0';
                char fileType[32];
                LOG_PRINT(LOG_DEBUG, "x_on_header_value File[%s]", filename);
                if (get_type(filename, fileType) == -1)
                {
                    LOG_PRINT(LOG_DEBUG, "x_on_header_value Get Type of File[%s] Failed!", filename);
                    mp_arg->check_name = -1;
                }
                else
                {
                    LOG_PRINT(LOG_DEBUG, "x_on_header_value fileType[%s]", fileType);
                    if (is_img(fileType) != 1)
                    {
                        LOG_PRINT(LOG_DEBUG, "x_on_header_value fileType[%s] is Not Supported!", fileType);
                        mp_arg->check_name = -1;
                    }
                }
            }
        }
        if (filename[0] != '\0' && mp_arg->check_name == -1)
        {
            LOG_PRINT(LOG_ERROR, "x_on_header_value %s fail post type", mp_arg->address);
            LinkedList *list = (LinkedList *)x_multipart_parser_get_info(p);

            img_info *info = (img_info *)malloc(sizeof(img_info));
            info->code = 201;
            info->size = 0;
            info->height = 0;
            info->width = 0;
            strcpy(info->md5, "");
            strcpy(info->msg, "File type is not supported!");
            addHead(list, info);
        }
    }
    //multipart_parser_set_data(p, mp_arg);
    char *header_value = (char *)malloc(length + 1);
    snprintf(header_value, length + 1, "%s", at);
    LOG_PRINT(LOG_DEBUG, "x_on_header_value header_value %d %s", length, header_value);
    free(header_value);
    return 0;
}

int x_on_chunk_data(x_multipart_parser *p, const char *at, size_t length)
{
    LOG_PRINT(LOG_DEBUG, "x_on_chunk_data excute");
    mp_arg_t *mp_arg = (mp_arg_t *)x_multipart_parser_get_data(p);
    mp_arg->partno++;
    LinkedList *list = (LinkedList *)x_multipart_parser_get_info(p);
    LOG_PRINT(LOG_DEBUG, "x_on_chunk_data LinkedList");
    LOG_PRINT(LOG_DEBUG, "x_on_chunk_data img_info");
    if (mp_arg->check_name == -1)
    {
        mp_arg->check_name = 0;
        return 0;
    }
    if (length < 1)
        return 0;
    //multipart_parser_set_data(p, mp_arg);

    img_info *info = (img_info *)malloc(sizeof(img_info));
    if (!info)
    {
        LOG_PRINT(LOG_DEBUG, "x_on_chunk_data malloc img_info error");
        return 0;
    }
    char md5sum[33];
    if (save_img(mp_arg->thr_arg, at, length, md5sum) == -1)
    {
        LOG_PRINT(LOG_DEBUG, "x_on_chunk_data Image Save Failed!");
        LOG_PRINT(LOG_ERROR, "x_on_chunk_data %s fail post save", mp_arg->address);
        info->code = 201;
        info->size = 0;
        info->height = 0;
        info->width = 0;
        strcpy(info->msg, "File save failed!");
        strcpy(info->md5, "");
        addHead(list, info);
    }
    else
    {
        mp_arg->succno++;
        LOG_PRINT(LOG_INFO, "x_on_chunk_data %s succ post pic:%s size:%d", mp_arg->address, md5sum, length);

        info->code = 200;
        info->size = (int)length;
        strcpy(info->md5, md5sum);
        strcpy(info->msg, "ok");
        info->width = 0;
        info->height = 0;
        addHead(list, info);
        LOG_PRINT(LOG_INFO, "x_on_chunk_data addHead  ");
    }
    return 0;
}

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
    char *ret = NULL;
    LinkedList *list = NULL;
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
    temp = malloc(strlen(base64) + 1);
    int len = url_decode(base64, temp);
    LOG_PRINT(LOG_DEBUG, "end url decode");

    int str_len = len / 4 * 3;
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
    LOG_PRINT(LOG_INFO, "%s succ post pic:%s size:%d", address, md5sum, length);
    list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);
    img_info *info = (img_info *)malloc(sizeof(img_info));
    info->code = 200;
    info->size = length;
    info->height = 0;
    info->width = 0;
    strcpy(info->md5, md5sum);
    strcpy(info->msg, "ok");
    addHead(list, info);
    ret = result2json(list);
    evbuffer_add_printf(req->buffer_out, "%s", ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============base64_request_cb() SUCC!===============");
    goto done;
err:
    if (temp != NULL)
    {
        free(temp);
        temp = NULL;
    }
    if (buff != NULL)
    {
        free(buff);
        buff = NULL;
    }
    list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);
    ret = result2json(list);
    evbuffer_add_printf(req->buffer_out, "%s", ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    free(ret);
    ret = NULL;
    LOG_PRINT(LOG_DEBUG, "============base64_request_cb() ERROR!===============");
done:
    if (ret)
    {
        free(ret);
        ret = NULL;
    }
    if (temp != NULL)
    {
        free(temp);
        temp = NULL;
    }
    if (buff != NULL)
    {
        free(buff);
        buff = NULL;
    }
    LOG_PRINT(LOG_DEBUG, "============base64_request_cb() DONE!===============");
}

int x_binary_parse(evhtp_request_t *req, const char *content_type, const char *address, const char *buff, int post_size)
{
    int err_no = 0;
    if (is_img(content_type) != 1)
    {
        LOG_PRINT(LOG_DEBUG, "fileType[%s] is Not Supported!", content_type);
        LOG_PRINT(LOG_ERROR, "%s fail post type", address);
        err_no = 1;
        goto done;
    }
    char md5sum[33];
    LOG_PRINT(LOG_DEBUG, "Begin to Save Image...");
    evthr_t *thread = get_request_thr(req);
    thr_arg_t *thr_arg = (thr_arg_t *)evthr_get_aux(thread);
    if (save_img(thr_arg, buff, post_size, md5sum) == -1)
    {
        LOG_PRINT(LOG_DEBUG, "Image Save Failed!");
        LOG_PRINT(LOG_ERROR, "%s fail post save", address);
        goto done;
    }
    err_no = -1;
    LOG_PRINT(LOG_INFO, "%s succ post pic:%s size:%d", address, md5sum, post_size);

    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);

    img_info *info = (img_info *)malloc(sizeof(img_info));
    info->code = 200;
    info->size = post_size;
    strcpy(info->md5, md5sum);
    strcpy(info->msg, "ok");
    info->height = 0;
    info->width = 0;
    addHead(list, info);
    char *ret = result2json(list);
    evbuffer_add_printf(req->buffer_out, "%s", ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    free(ret);
done:
    return err_no;
}
int x_multipart_parse(evhtp_request_t *req, const char *content_type, const char *address, const char *buff, int post_size)
{
    int err_no = 0;
    char *boundary = NULL, *boundary_end = NULL;
    char *boundaryPattern = NULL;
    int boundary_len = 0;
    char *ret = NULL;
    mp_arg_t *mp_arg = NULL;
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);

    if (strstr(content_type, "boundary") == 0)
    {
        LOG_PRINT(LOG_DEBUG, "x_multipart_parse boundary NOT found!");
        LOG_PRINT(LOG_ERROR, "x_multipart_parse %s fail post parse", address);
        err_no = 6;
        goto done;
    }

    boundary = strchr(content_type, '=');
    boundary++;
    boundary_len = strlen(boundary);

    if (boundary[0] == '"')
    {
        boundary++;
        boundary_end = strchr(boundary, '"');
        if (!boundary_end)
        {
            LOG_PRINT(LOG_DEBUG, "x_multipart_parse Invalid boundary in multipart/form-data POST data");
            LOG_PRINT(LOG_ERROR, "x_multipart_parse %s fail post parse", address);
            err_no = 6;
            goto done;
        }
    }
    else
    {
        /* search for the end of the boundary */
        boundary_end = strpbrk(boundary, ",;");
    }
    if (boundary_end)
    {
        boundary_end[0] = '\0';
        boundary_len = boundary_end - boundary;
    }

    LOG_PRINT(LOG_DEBUG, "x_multipart_parse boundary Find. boundary = %s", boundary);
    boundaryPattern = (char *)malloc(boundary_len + 3);
    if (boundaryPattern == NULL)
    {
        LOG_PRINT(LOG_DEBUG, "x_multipart_parse boundarypattern malloc failed!");
        LOG_PRINT(LOG_ERROR, "x_multipart_parse %s fail malloc", address);
        err_no = 0;
        goto done;
    }
    snprintf(boundaryPattern, boundary_len + 3, "--%s", boundary);
    LOG_PRINT(LOG_DEBUG, "x_multipart_parse boundaryPattern = %s, strlen = %d", boundaryPattern, (int)strlen(boundaryPattern));
    LOG_PRINT(LOG_DEBUG, "x_multipart_parse %d post_size", post_size);

    x_multipart_parser *parser = x_multipart_parser_init(boundaryPattern);
    if (!parser)
    {
        LOG_PRINT(LOG_DEBUG, "x_multipart_parse Multipart_parser Init Failed!");
        LOG_PRINT(LOG_ERROR, "x_multipart_parse %s fail post save", address);
        err_no = 0;
        goto done;
    }
    mp_arg = (mp_arg_t *)malloc(sizeof(mp_arg_t));
    if (!mp_arg)
    {
        LOG_PRINT(LOG_DEBUG, "x_multipart_parse Multipart_parser Arg Malloc Failed!");
        LOG_PRINT(LOG_ERROR, "x_multipart_parse %s fail post save", address);
        err_no = 0;
        goto done;
    }

    evthr_t *thread = get_request_thr(req);
    thr_arg_t *thr_arg = (thr_arg_t *)evthr_get_aux(thread);
    mp_arg->req = req;
    mp_arg->thr_arg = thr_arg;
    str_lcpy(mp_arg->address, address, 16);
    mp_arg->partno = 0;
    mp_arg->succno = 0;
    mp_arg->check_name = 0;
    x_multipart_parser_set_data(parser, mp_arg);
    x_multipart_parser_set_info(parser, list);
    x_multipart_parser_execute(parser, buff, post_size);
    // displayLinkedList(list, (DISPLAY)displayInfo);
    ret = result2json(list);
    x_multipart_parser_free(parser);

    evbuffer_add_printf(req->buffer_out, ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));

    // evbuffer_add_printf(req->buffer_out, "</body>\n</html>\n");
    err_no = -1;

done:
    free(boundaryPattern);
    free(mp_arg);
    if (ret != NULL)
    {
        free(ret);
    }
    return err_no;
}
void post_upload_request_cb(evhtp_request_t *req, void *arg)
{
    int post_size = 0;
    char *buff = NULL;
    int err_no = 0;
    char *ret = NULL;
    LinkedList *list = NULL;

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

    if (settings.up_access != NULL)
    {
        int acs = zimg_access_inet(settings.up_access, ss->sin_addr.s_addr);
        LOG_PRINT(LOG_DEBUG, "access check: %d", acs);
        if (acs == ZIMG_FORBIDDEN)
        {
            LOG_PRINT(LOG_DEBUG, "check access: ip[%s] forbidden!", address);
            LOG_PRINT(LOG_INFO, "%s refuse post forbidden", address);
            err_no = 3;
            goto forbidden;
        }
        else if (acs == ZIMG_ERROR)
        {
            LOG_PRINT(LOG_DEBUG, "check access: check ip[%s] failed!", address);
            LOG_PRINT(LOG_ERROR, "%s fail post access %s", address);
            err_no = 0;
            goto err;
        }
    }

    const char *content_len = evhtp_header_find(req->headers_in, "Content-Length");
    if (!content_len)
    {
        LOG_PRINT(LOG_DEBUG, "Get Content-Length error!");
        LOG_PRINT(LOG_ERROR, "%s fail post content-length", address);
        err_no = 5;
        goto err;
    }
    post_size = atoi(content_len);
    if (post_size <= 0)
    {
        LOG_PRINT(LOG_DEBUG, "Image Size is Zero!");
        LOG_PRINT(LOG_ERROR, "%s fail post empty", address);
        err_no = 5;
        goto err;
    }
    if (post_size > settings.max_size)
    {
        LOG_PRINT(LOG_DEBUG, "Image Size Too Large!");
        LOG_PRINT(LOG_ERROR, "%s fail post large", address);
        err_no = 7;
        goto err;
    }
    const char *content_type = evhtp_header_find(req->headers_in, "Content-Type");
    if (!content_type)
    {
        LOG_PRINT(LOG_DEBUG, "Get Content-Type error!");
        LOG_PRINT(LOG_ERROR, "%s fail post content-type", address);
        err_no = 6;
        goto err;
    }
    evbuf_t *buf;
    buf = req->buffer_in;
    buff = (char *)malloc(post_size);
    if (buff == NULL)
    {
        LOG_PRINT(LOG_DEBUG, "buff malloc failed!");
        LOG_PRINT(LOG_ERROR, "%s fail malloc buff", address);
        err_no = 0;
        goto err;
    }
    int rmblen, evblen;
    if (evbuffer_get_length(buf) <= 0)
    {
        LOG_PRINT(LOG_DEBUG, "Empty Request!");
        LOG_PRINT(LOG_ERROR, "%s fail post empty", address);
        err_no = 4;
        goto err;
    }
    while ((evblen = evbuffer_get_length(buf)) > 0)
    {
        LOG_PRINT(LOG_DEBUG, "evblen = %d", evblen);
        rmblen = evbuffer_remove(buf, buff, evblen);
        LOG_PRINT(LOG_DEBUG, "rmblen = %d", rmblen);
        if (rmblen < 0)
        {
            LOG_PRINT(LOG_DEBUG, "evbuffer_remove failed!");
            LOG_PRINT(LOG_ERROR, "%s fail post parse", address);
            err_no = 4;
            goto err;
        }
    }
    if (strstr(content_type, "multipart/form-data") == NULL)
    {
        err_no = x_binary_parse(req, content_type, address, buff, post_size);
    }
    else
    {
        err_no = x_multipart_parse(req, content_type, address, buff, post_size);
    }
    if (err_no != -1)
    {
        goto err;
    }
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============post_upload_request_cb() DONE!===============");
    goto done;

forbidden:
    list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);
    ret = result2json(list);
    evbuffer_add_printf(req->buffer_out, "%s", ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============post_upload_request_cb() FORBIDDEN!===============");
    goto done;

err:
    list = (LinkedList *)malloc(sizeof(LinkedList));
    initializeList(list);
    ret = result2json(list);
    evbuffer_add_printf(req->buffer_out, "%s", ret);
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "application/json", 0, 0));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", settings.server_name, 0, 1));
    evhtp_send_reply(req, EVHTP_RES_OK);
    LOG_PRINT(LOG_DEBUG, "============post_upload_request_cb() ERROR!===============");

done:
    free(buff);
    if (ret != NULL)
    {
        free(ret);
    }
}
