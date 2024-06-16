#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t count);
void *memset(void *dest, int c, size_t count);

char *strcat(char *dest, char *src);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);
char *strcpy(char *dest, char *src);

#endif
