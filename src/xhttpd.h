#ifndef XHTTPD_H
#define XHTTPD_H

#include <wand/magick_wand.h>
#include "libevhtp/evhtp.h"
#include "xparser.h"
void post_upload_request_cb(evhtp_request_t *req, void *arg);
void base64_request_cb(evhtp_request_t *req, void *arg);

int x_on_header_field(x_multipart_parser *p, const char *at, size_t length);
int x_on_header_value(x_multipart_parser *p, const char *at, size_t length);
int x_on_chunk_data(x_multipart_parser *p, const char *at, size_t length);
#endif