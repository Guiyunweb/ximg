/* Based on node-formidable by Felix Geisendörfer 
 * Igor Afonov - afonov@gmail.com - 2012
 * MIT License - http://www.opensource.org/licenses/mit-license.php
 */
#ifndef XPARSER_H
#define XPARSER_H

#include <stdlib.h>
#include <ctype.h>

typedef struct x_multipart_parser x_multipart_parser;
typedef struct x_multipart_parser_settings x_multipart_parser_settings;
typedef struct x_multipart_parser_state x_multipart_parser_state;

typedef int (*x_multipart_data_cb)(x_multipart_parser *, const char *at, size_t length);
typedef int (*x_multipart_notify_cb)(x_multipart_parser *);

struct x_multipart_parser_settings
{
    x_multipart_data_cb x_on_header_field;
    x_multipart_data_cb x_on_header_value;
    x_multipart_data_cb x_on_part_data;
    x_multipart_data_cb x_on_chunk_data;

    x_multipart_notify_cb x_on_part_data_begin;
    x_multipart_notify_cb x_on_headers_complete;
    x_multipart_notify_cb x_on_part_data_end;
    x_multipart_notify_cb x_on_body_end;
};

x_multipart_parser *x_multipart_parser_init(const char *boundary);

void x_multipart_parser_free(x_multipart_parser *p);

size_t x_multipart_parser_execute(x_multipart_parser *p, const char *buf, size_t len);

void x_multipart_parser_set_data(x_multipart_parser *p, void *data);
void *x_multipart_parser_get_data(x_multipart_parser *p);

void x_multipart_parser_set_info(x_multipart_parser *p, void *info);
void *x_multipart_parser_get_info(x_multipart_parser *p);
#endif
