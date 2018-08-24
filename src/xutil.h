#ifndef XUTIL_H
#define XUTIL_H
#include "xlist.h"

typedef struct
{
    int code;
    int size;
    int height;
    int width;
    char md5[33];
    char msg[100];
} img_info;

int b64_int(int ch);
int b64_decode(const char *in, int in_len, char *out);
/***url解码**/
int url_decode(const char *src, char *dest);
/**链表转为json字符串*/
char *result2json(LinkedList *list);

#endif