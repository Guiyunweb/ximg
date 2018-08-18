#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zurl.h"

/* Function: urlDecode */
char *url_decode(const char *str)
{
    char *dest = malloc(strlen(str) + 1);
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
    return dest;
}
