#include "helper.h"

int my_strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void write_str(int fd, const char *str) {
    write(fd, str, my_strlen(str));
}

int str_to_int(const char *str) {
    int result = 0;
    int i = 0;
    int sign = 1;

    if (str[0] == '-') {
        sign = -1;
        i = 1;
    }

    while (str[i] != '\0' && str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result * sign;
}

int my_strcasecmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        char c1 = s1[i];
        char c2 = s2[i];

        if (c1 >= 'a' && c1 <= 'z') c1 = c1 - 'a' + 'A';
        if (c2 >= 'a' && c2 <= 'z') c2 = c2 - 'a' + 'A';

        if (c1 != c2) return c1 - c2;
        i++;
    }
    return s1[i] - s2[i];
}

void int_to_str(int num, char *buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char temp[20];
    int i = 0;
    int is_negative = 0;
    int n = num;

    if (num < 0) {
        is_negative = 1;
        n = -num;
    }

    while (n > 0) {
        temp[i++] = '0' + (n % 10);
        n /= 10;
    }

    int j = 0;
    if (is_negative) buffer[j++] = '-';

    while (i > 0) {
        i--;
        buffer[j++] = temp[i];
    }
    buffer[j] = '\0';
}

void str_append(char *dest, const char *src) {
    int dest_len = my_strlen(dest);
    int i = 0;
    while (src[i] != '\0') {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
}

void my_strcpy(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void str_tolower(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        if (src[i] >= 'A' && src[i] <= 'Z') {
            dest[i] = src[i] + ('a' - 'A');
        } else {
            dest[i] = src[i];
        }
        i++;
    }
    dest[i] = '\0';
}

int my_strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

void die(char *msg){
    write_str(STDERR_FILENO, msg);
    exit(1);
}

int read_line_fd(int fd, char *buffer, int max_len) {
    int i = 0; char c;
    while (i < max_len - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
        if (c == '\n') break;
        if (c != '\r') buffer[i++] = c; // ignore CR
    }
    buffer[i] = '\0';
    return i;
}

void clean_realm_name(char *name) {
    int i = 0, j = 0;
    while (name[i] != '\0') {
        if (name[i] != '&') name[j++] = name[i];
        i++;
    }
    name[j] = '\0';
}
