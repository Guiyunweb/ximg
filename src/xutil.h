#ifndef XUTIL_H
#define XUTIL_H
int b64_int(int ch);
int b64_decode(const char *in, int in_len, char *out);
/***url解码**/
int url_decode(const char *src,char *dest);
#endif