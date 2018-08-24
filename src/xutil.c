#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xutil.h"
#include "zlog.h"
#include "cjson/cJSON.h"
const char b64_chr[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int b64_int(int ch)
{

    // ASCII to base64_int
    // 65-90  Upper Case  >>  0-25
    // 97-122 Lower Case  >>  26-51
    // 48-57  Numbers     >>  52-61
    // 43     Plus (+)    >>  62
    // 47     Slash (/)   >>  63
    // 61     Equal (=)   >>  64~
    if (ch == 43)
        return 62;
    if (ch == 47)
        return 63;
    if (ch == 61)
        return 64;
    if ((ch > 47) && (ch < 58))
        return ch + 4;
    if ((ch > 64) && (ch < 91))
        return ch - 'A';
    if ((ch > 96) && (ch < 123))
        return (ch - 'a') + 26;
    return 0;
}
int b64_decode(const char *in, int in_len, char *out)
{
    int i = 0, j = 0, k = 0, s[4];

    for (i = 0; i < in_len; i++)
    {
        s[j++] = b64_int(*(in + i));
        if (j == 4)
        {
            out[k + 0] = (char)(((s[0] & 255) << 2) + ((s[1] & 0x30) >> 4));
            if (s[2] != 64)
            {
                out[k + 1] = (char)(((s[1] & 0x0F) << 4) + ((s[2] & 0x3C) >> 2));
                if ((s[3] != 64))
                {
                    out[k + 2] = (char)(((s[2] & 0x03) << 6) + (s[3]));
                    k += 3;
                }
                else
                {
                    k += 2;
                }
            }
            else
            {
                k += 1;
            }
            j = 0;
        }
    }

    return k;
}

int url_decode(const char *str, char *dest)
{
    char e_str[] = "00";
    int j = 0;
    for (int i = 0, len = strlen(str); i < len; i++)
    {
        if (str[i] == '%')
        {
            if (str[i + 1] == 0)
            {
                dest[j] = str[i];
                j++;
                continue;
            }
            if (isxdigit(str[i + 1]) && isxdigit(str[i + 2]))
            {
                e_str[0] = str[i + 1];
                e_str[1] = str[i + 2];
                long int x = strtol(e_str, NULL, 16);
                dest[j] = x;
                j++;
                i += 2;
            }
        }
        else
        {
            dest[j] = str[i];
            j++;
        }
    }
    return j;
}
void operatorInfo(img_info *info, cJSON *array)
{
    cJSON *j_ret_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(j_ret_info, "code", info->code);
    cJSON_AddNumberToObject(j_ret_info, "size", info->size);
    cJSON_AddStringToObject(j_ret_info, "md5", info->md5);
    cJSON_AddStringToObject(j_ret_info, "msg", info->msg);
    cJSON_AddNumberToObject(j_ret_info, "width", info->width);
    cJSON_AddNumberToObject(j_ret_info, "height", info->height);
    cJSON_AddItemToArray(array, j_ret_info);
    free(info);
}
char *result2json(LinkedList *list)
{
    cJSON *j_ret = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    if (list)
    {
        operatorLinkedList(list, (OPERATOR)operatorInfo, array);
        cJSON_AddNumberToObject(j_ret, "code", 200);
    }
    else
    {
        cJSON_AddNumberToObject(j_ret, "code", 201);
    }
    cJSON_AddItemToObject(j_ret, "data", array);
    char *ret = cJSON_PrintUnformatted(j_ret);
    cJSON_Delete(j_ret);
    freeLinkedList(list);
    return ret;
}
