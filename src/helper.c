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

void long_to_str(long long num, char *buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char temp[32];
    int i = 0;
    int is_negative = 0;
    long long n = num;

    if (n < 0) {
        is_negative = 1;
        n = -n;
    }

    while (n > 0 && i < (int)sizeof(temp)) {
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

void ulong_to_str(unsigned long long num, char *buffer) {
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char temp[32];
    int i = 0;
    while (num > 0 && i < (int)sizeof(temp)) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }

    int j = 0;
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

int safe_append(char *dest, size_t dest_size, const char *src) {
    if (dest == NULL || src == NULL || dest_size == 0) return -1;
    size_t dest_len = my_strlen(dest);
    size_t src_len = my_strlen(src);
    if (dest_len + src_len + 1 > dest_size) {
        return -1;
    }
    memcpy(dest + dest_len, src, src_len + 1);
    return 0;
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

// Helper function to convert hex character to value
static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Compute MD5 using fork/exec/pipe to call md5sum command (per statement requirement)
int md5_compute_file(const char* path, uint8_t digest[16]) {
    if (path == NULL || digest == NULL) {
        return -1;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close read end

        // Redirect stdout to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(1);
        }
        close(pipefd[1]);

        // Execute md5sum
        char* args[] = {"md5sum", (char*)path, NULL};
        execvp("md5sum", args);

        // If execvp returns, it failed
        _exit(1);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Read output from md5sum (format: "hash  filename\n")
    char output[128];
    ssize_t total = 0;
    while (total < (ssize_t)sizeof(output) - 1) {
        ssize_t n = read(pipefd[0], output + total, sizeof(output) - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    output[total] = '\0';
    close(pipefd[0]);

    // Wait for child process
    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }

    // Parse the 32 hex characters from output
    if (total < 32) {
        return -1;
    }

    for (int i = 0; i < 16; i++) {
        int hi = hex_char_to_val(output[i * 2]);
        int lo = hex_char_to_val(output[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        digest[i] = (uint8_t)((hi << 4) | lo);
    }

    return 0;
}

void md5_digest_to_hex(const uint8_t digest[16], char hex[33]) {
    static const char* digits = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        hex[i * 2]     = digits[(digest[i] >> 4) & 0xF];
        hex[i * 2 + 1] = digits[digest[i] & 0xF];
    }
    hex[32] = '\0';
}
