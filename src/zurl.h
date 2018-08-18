
#ifndef ZURL_H
#define ZURL_H

/*
 * Function: url_decode
 * Purpose:  Decodes a web-encoded URL. By default, +'s are converted to spaces.
 * Input:    const char* str - the URL to decode
 * Output:   char* - the decoded URL
 */
char *url_decode(const char *str);

#endif