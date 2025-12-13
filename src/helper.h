#ifndef HELPER_H
#define HELPER_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/wait.h>

// String utility functions
int  my_strlen(const char *str);
void write_str(int fd, const char *str);
int  str_to_int(const char *str);
int  my_strcasecmp(const char *s1, const char *s2);
int  my_strcmp(const char *s1, const char *s2);
void my_strcpy(char *dest, const char *src);
void str_tolower(char *dest, const char *src);
void int_to_str(int num, char *buffer);
void long_to_str(long long num, char *buffer);
void ulong_to_str(unsigned long long num, char *buffer);
void str_append(char *dest, const char *src);
int  safe_append(char *dest, size_t dest_size, const char *src);
int  read_line_fd(int fd, char *buffer, int max_len);
void clean_realm_name(char *name);

// Error handling
void die(char *msg);

// MD5 utilities (uses fork/exec to call md5sum command per statement requirement)
int  md5_compute_file(const char* path, uint8_t digest[16]);
void md5_digest_to_hex(const uint8_t digest[16], char hex[33]);

#endif
