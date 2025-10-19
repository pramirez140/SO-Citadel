#ifndef HELPER_H
#define HELPER_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// String utility functions
int  my_strlen(const char *str);
void write_str(int fd, const char *str);
int  str_to_int(const char *str);
int  my_strcasecmp(const char *s1, const char *s2);
int  my_strcmp(const char *s1, const char *s2);
void my_strcpy(char *dest, const char *src);
void str_tolower(char *dest, const char *src);
void int_to_str(int num, char *buffer);
void str_append(char *dest, const char *src);

// Error handling
void die(char *msg);

#endif
