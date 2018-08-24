
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "zcommon.h"
#include "xparser.h"

#define X_NOTIFY_CB(FOR)                         \
    do                                           \
    {                                            \
        if (p->settings->x_on_##FOR)             \
        {                                        \
            if (p->settings->x_on_##FOR(p) != 0) \
            {                                    \
                return i;                        \
            }                                    \
        }                                        \
    } while (0)

#define X_EMIT_DATA_CB(FOR, ptr, len)                      \
    do                                                     \
    {                                                      \
        if (p->settings->x_on_##FOR)                       \
        {                                                  \
            if (p->settings->x_on_##FOR(p, ptr, len) != 0) \
            {                                              \
                return i;                                  \
            }                                              \
        }                                                  \
    } while (0)

#define LF 10
#define CR 13

struct x_multipart_parser
{
    void *data;
    void *info;

    size_t index;
    size_t boundary_length;

    unsigned char x_state;

    const x_multipart_parser_settings *settings;

    char *lookbehind;
    char x_multipart_boundary[1];
};

enum x_state
{
    s_uninitialized = 1,
    s_start,
    s_start_boundary,
    s_header_field_start,
    s_header_field,
    s_headers_almost_done,
    s_header_value_start,
    s_header_value,
    s_header_value_almost_done,
    s_part_data_start,
    s_part_data,
    s_part_data_almost_boundary,
    s_part_data_boundary,
    s_part_data_almost_end,
    s_part_data_end,
    s_part_data_final_hyphen,
    s_end
};

x_multipart_parser *x_multipart_parser_init(const char *boundary)
{

    x_multipart_parser *p = malloc(sizeof(x_multipart_parser) +
                                   strlen(boundary) +
                                   strlen(boundary) + 9);

    strcpy(p->x_multipart_boundary, boundary);
    p->boundary_length = strlen(boundary);

    p->lookbehind = (p->x_multipart_boundary + p->boundary_length + 1);

    p->index = 0;
    p->x_state = s_start;
    p->settings = settings.x_mp_set;

    return p;
}

void x_multipart_parser_free(x_multipart_parser *p)
{
    free(p);
}

void x_multipart_parser_set_data(x_multipart_parser *p, void *data)
{
    p->data = data;
}
void *x_multipart_parser_get_data(x_multipart_parser *p)
{
    return p->data;
}

void x_multipart_parser_set_info(x_multipart_parser *p, void *info)
{
    p->info = info;
}
void *x_multipart_parser_get_info(x_multipart_parser *p)
{
    return p->info;
}
size_t x_multipart_parser_execute(x_multipart_parser *p, const char *buf, size_t len)
{
    LOG_PRINT(LOG_ERROR, "x_multipart_parser_execute method excuete");
    size_t i = 0;
    size_t mark = 0;
    size_t dmark = 0, dbmark = 0;
    char c, cl;
    int is_last = 0;

    while (i < len)
    {
        c = buf[i];
        is_last = (i == (len - 1));
        switch (p->x_state)
        {
        case s_start:
            p->index = 0;
            p->x_state = s_start_boundary;

        /* fallthrough */
        case s_start_boundary:
            if (p->index == p->boundary_length)
            {
                if (c != CR)
                {
                    return i;
                }
                p->index++;
                break;
            }
            else if (p->index == (p->boundary_length + 1))
            {
                if (c != LF)
                {
                    return i;
                }
                p->index = 0;
                X_NOTIFY_CB(part_data_begin);
                p->x_state = s_header_field_start;
                break;
            }
            if (c != p->x_multipart_boundary[p->index])
            {
                return i;
            }
            p->index++;
            break;

        case s_header_field_start:
            mark = i;
            p->x_state = s_header_field;

        /* fallthrough */
        case s_header_field:
            if (c == CR)
            {
                p->x_state = s_headers_almost_done;
                break;
            }

            if (c == '-')
            {
                break;
            }

            if (c == ':')
            {
                X_EMIT_DATA_CB(header_field, buf + mark, i - mark);
                p->x_state = s_header_value_start;
                break;
            }

            cl = tolower(c);
            if (cl < 'a' || cl > 'z')
            {
                return i;
            }
            if (is_last)
                X_EMIT_DATA_CB(header_field, buf + mark, (i - mark) + 1);
            break;

        case s_headers_almost_done:
            if (c != LF)
            {
                return i;
            }

            p->x_state = s_part_data_start;
            break;

        case s_header_value_start:
            if (c == ' ')
            {
                break;
            }

            mark = i;
            p->x_state = s_header_value;

        /* fallthrough */
        case s_header_value:
            if (c == CR)
            {
                X_EMIT_DATA_CB(header_value, buf + mark, i - mark);
                p->x_state = s_header_value_almost_done;
            }
            if (is_last)
                X_EMIT_DATA_CB(header_value, buf + mark, (i - mark) + 1);
            break;

        case s_header_value_almost_done:
            if (c != LF)
            {
                return i;
            }
            p->x_state = s_header_field_start;
            break;

        case s_part_data_start:
            X_NOTIFY_CB(headers_complete);
            mark = i;
            dmark = i;
            //multipart_log("s_part_data_start @ %d", dmark);
            p->x_state = s_part_data;

        /* fallthrough */
        case s_part_data:
            if (c == CR)
            {
                mark = i;
                dbmark = i;
                p->x_state = s_part_data_almost_boundary;
                p->lookbehind[0] = CR;
                break;
            }
            break;

        case s_part_data_almost_boundary:
            if (c == LF)
            {
                p->x_state = s_part_data_boundary;
                p->lookbehind[1] = LF;
                p->index = 0;
                break;
            }
            p->x_state = s_part_data;
            mark = i--;
            break;

        case s_part_data_boundary:
            if (p->x_multipart_boundary[p->index] != c)
            {
                p->x_state = s_part_data;
                mark = i--;
                break;
            }
            p->lookbehind[2 + p->index] = c;
            if ((++p->index) == p->boundary_length)
            {
                X_NOTIFY_CB(part_data_end);
                p->x_state = s_part_data_almost_end;
            }
            break;

        case s_part_data_almost_end:
            if (c == '-')
            {
                p->x_state = s_part_data_final_hyphen;
                break;
            }
            if (c == CR)
            {
                p->x_state = s_part_data_end;
                break;
            }
            return i;

        case s_part_data_final_hyphen:
            //multipart_log("s_part_data_final_hyphen %d~%d", dmark, dbmark);
            X_EMIT_DATA_CB(chunk_data, buf + dmark, dbmark - dmark);
            if (c == '-')
            {
                X_NOTIFY_CB(body_end);
                p->x_state = s_end;
                break;
            }
            return i;

        case s_part_data_end:
            if (c == LF)
            {
                //multipart_log("s_part_data_end %d~%d", dmark, dbmark);
                X_EMIT_DATA_CB(chunk_data, buf + dmark, dbmark - dmark);
                p->x_state = s_header_field_start;
                X_NOTIFY_CB(part_data_begin);
                break;
            }
            return i;

        case s_end:
            break;

        default:
            return 0;
        }
        ++i;
    }

    return len;
}
