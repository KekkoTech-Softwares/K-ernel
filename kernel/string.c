/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 KekkoTech Softwares Open Source (Matteo Checcacci)
 *
 * string.c — memory manipulation and strings.
 *
 * GCC can creat a memset/memcpy/memmove/memcmp call even without the code calling
 * them (for copying structs, array initializations) so they must exist
*/

#include "string.h"

void * memset(void * dest, int c, size_t n) {

    unsigned char * p = (unsigned char *)dest; //unsigned char because i dunnow how big an element is

    for(size_t i = 0; i < n; i++)
        p[i] = (unsigned char)c;

    return dest;
}

void * memcpy(void * dest, const void * src, size_t n) {

    unsigned char * d = (unsigned char *)dest;
    const unsigned char * s = (const unsigned char *)src;

    for(size_t i = 0; i < n; i++)
        d[i] = s[i];

        return dest;
}

void * memmove(void * dest, const void * src, size_t n) {

    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if(d < s){//no dangerous overwriting, copy ahead
        for(size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else { /*d comes afer s, copy ahead will overwrite bytes I still didn't read so i must start from the bottom.
        note: the for starts from i = n and condition is i > 0, with access at * [i-1]. With size_t writing "i >= 0" it will be
        an infinite loop, beacause unsigned is never negative. After 0 go back to maximum value. I hope if you are readung this you are understanding something 
        beacause in a week i'll forget what i did */
        for(size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dest;
}

int memcmp(const void *a, const void * b, size_t n) {

    const unsigned char * pa = (const unsigned char *)a;
    const unsigned char * pb = (const unsigned char *)b;

    for (size_t i = 0; i < n; i++) {
        if(pa[i] != pb[i])
        return (int)pa[i] - (int)pb[i];
    }
    
    return 0;
}

size_t strlen(const char * s) {
    
    size_t len = 0;

    while (s[len])
        len++;
    return len;
}

int strcmp(const char * a, const char * b) {
    while (* a && * a == * b) {
        a++;
        b++;
    }

    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}