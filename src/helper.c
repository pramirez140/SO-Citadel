#include "helper.h"
#include <fcntl.h>
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

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

#define F(x,y,z) (((x) & (y)) | ((~x) & (z)))
#define G(x,y,z) (((x) & (z)) | ((y) & (~z)))
#define H(x,y,z) ((x) ^ (y) ^ (z))
#define I(x,y,z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x,n) (((x) << (n)) | ((x) >> (32-(n))))

#define FF(a,b,c,d,x,s,ac) { \
  (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
  (a) = ROTATE_LEFT ((a), (s)); \
  (a) += (b); \
}
#define GG(a,b,c,d,x,s,ac) { \
  (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
  (a) = ROTATE_LEFT ((a), (s)); \
  (a) += (b); \
}
#define HH(a,b,c,d,x,s,ac) { \
  (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
  (a) = ROTATE_LEFT ((a), (s)); \
  (a) += (b); \
}
#define II(a,b,c,d,x,s,ac) { \
  (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
  (a) = ROTATE_LEFT ((a), (s)); \
  (a) += (b); \
}

static void MD5Transform(uint32_t state[4], const uint8_t block[64]);
static void Encode(uint8_t *output, const uint32_t *input, size_t len);
static void Decode(uint32_t *output, const uint8_t *input, size_t len);
static void MD5_memcpy(uint8_t *output, const uint8_t *input, size_t len);
static void MD5_memset(uint8_t *output, int value, size_t len);

static const uint8_t PADDING[64] = { 0x80 };

void md5_init(MD5Context* ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

void md5_update(MD5Context* ctx, const uint8_t* input, size_t len) {
    size_t index = (ctx->count[0] >> 3) & 0x3F;
    ctx->count[0] += (uint32_t)(len << 3);
    if (ctx->count[0] < (len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32_t)(len >> 29);

    size_t partLen = 64 - index;
    size_t i = 0;
    if (len >= partLen) {
        MD5_memcpy(&ctx->buffer[index], input, partLen);
        MD5Transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < len; i += 64) {
            MD5Transform(ctx->state, &input[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    MD5_memcpy(&ctx->buffer[index], &input[i], len - i);
}

void md5_final(uint8_t digest[16], MD5Context* ctx) {
    uint8_t bits[8];
    Encode(bits, ctx->count, 8);

    size_t index = (ctx->count[0] >> 3) & 0x3f;
    size_t padLen = (index < 56) ? (56 - index) : (120 - index);
    md5_update(ctx, PADDING, padLen);
    md5_update(ctx, bits, 8);
    Encode(digest, ctx->state, 16);
    MD5_memset((uint8_t*)ctx, 0, sizeof(*ctx));
}

static void MD5Transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];
    Decode(x, block, 64);

    FF (a, b, c, d, x[ 0], S11, 0xd76aa478);
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756);
    FF (c, d, a, b, x[ 2], S13, 0x242070db);
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee);
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf);
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a);
    FF (c, d, a, b, x[ 6], S13, 0xa8304613);
    FF (b, c, d, a, x[ 7], S14, 0xfd469501);
    FF (a, b, c, d, x[ 8], S11, 0x698098d8);
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af);
    FF (c, d, a, b, x[10], S13, 0xffff5bb1);
    FF (b, c, d, a, x[11], S14, 0x895cd7be);
    FF (a, b, c, d, x[12], S11, 0x6b901122);
    FF (d, a, b, c, x[13], S12, 0xfd987193);
    FF (c, d, a, b, x[14], S13, 0xa679438e);
    FF (b, c, d, a, x[15], S14, 0x49b40821);

    GG (a, b, c, d, x[ 1], S21, 0xf61e2562);
    GG (d, a, b, c, x[ 6], S22, 0xc040b340);
    GG (c, d, a, b, x[11], S23, 0x265e5a51);
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa);
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d);
    GG (d, a, b, c, x[10], S22,  0x2441453);
    GG (c, d, a, b, x[15], S23, 0xd8a1e681);
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8);
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6);
    GG (d, a, b, c, x[14], S22, 0xc33707d6);
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87);
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed);
    GG (a, b, c, d, x[13], S21, 0xa9e3e905);
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8);
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9);
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a);

    HH (a, b, c, d, x[ 5], S31, 0xfffa3942);
    HH (d, a, b, c, x[ 8], S32, 0x8771f681);
    HH (c, d, a, b, x[11], S33, 0x6d9d6122);
    HH (b, c, d, a, x[14], S34, 0xfde5380c);
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44);
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9);
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60);
    HH (b, c, d, a, x[10], S34, 0xbebfbc70);
    HH (a, b, c, d, x[13], S31, 0x289b7ec6);
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa);
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085);
    HH (b, c, d, a, x[ 6], S34,  0x4881d05);
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039);
    HH (d, a, b, c, x[12], S32, 0xe6db99e5);
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8);
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665);

    II (a, b, c, d, x[ 0], S41, 0xf4292244);
    II (d, a, b, c, x[ 7], S42, 0x432aff97);
    II (c, d, a, b, x[14], S43, 0xab9423a7);
    II (b, c, d, a, x[ 5], S44, 0xfc93a039);
    II (a, b, c, d, x[12], S41, 0x655b59c3);
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92);
    II (c, d, a, b, x[10], S43, 0xffeff47d);
    II (b, c, d, a, x[ 1], S44, 0x85845dd1);
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f);
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0);
    II (c, d, a, b, x[ 6], S43, 0xa3014314);
    II (b, c, d, a, x[13], S44, 0x4e0811a1);
    II (a, b, c, d, x[ 4], S41, 0xf7537e82);
    II (d, a, b, c, x[11], S42, 0xbd3af235);
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb);
    II (b, c, d, a, x[ 9], S44, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    memset(x, 0, sizeof(x));
}

static void Encode(uint8_t *output, const uint32_t *input, size_t len) {
    for (size_t i = 0, j = 0; j < len; i++, j += 4) {
        output[j]   = (uint8_t)( input[i]        & 0xff);
        output[j+1] = (uint8_t)((input[i] >> 8)  & 0xff);
        output[j+2] = (uint8_t)((input[i] >> 16) & 0xff);
        output[j+3] = (uint8_t)((input[i] >> 24) & 0xff);
    }
}

static void Decode(uint32_t *output, const uint8_t *input, size_t len) {
    for (size_t i = 0, j = 0; j < len; i++, j += 4) {
        output[i] = ((uint32_t)input[j]) |
                    (((uint32_t)input[j+1]) << 8) |
                    (((uint32_t)input[j+2]) << 16) |
                    (((uint32_t)input[j+3]) << 24);
    }
}

static void MD5_memcpy(uint8_t *output, const uint8_t *input, size_t len) {
    memcpy(output, input, len);
}

static void MD5_memset(uint8_t *output, int value, size_t len) {
    memset(output, value, len);
}

int md5_compute_file(const char* path, uint8_t digest[16]) {
    if (path == NULL || digest == NULL) {
        return -1;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    MD5Context ctx;
    md5_init(&ctx);
    uint8_t buffer[4096];
    while (1) {
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes < 0) {
            close(fd);
            return -1;
        }
        if (bytes == 0) {
            break;
        }
        md5_update(&ctx, buffer, (size_t)bytes);
    }
    close(fd);
    md5_final(digest, &ctx);
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
