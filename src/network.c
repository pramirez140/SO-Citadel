#include "network.h"

static void frame_copy_field_padded(const char* src, uint8_t* dst, size_t field_len);
static void frame_extract_field(const uint8_t* src, size_t field_len, char* dst, size_t dst_len);
static void frame_store_field(char* dst, size_t dst_len, const char* src);
static Route* maester_find_route(Maester* maester, const char* realm);
static Route* maester_find_default_route(Maester* maester);
static int    maester_route_is_known(const Route* route);
static ConnectionEntry* maester_find_connection(Maester* maester, const char* realm);
ConnectionEntry* maester_add_connection_entry(Maester* maester);
static int    set_socket_nonblocking(int fd);
static int    maester_queue_bytes(ConnectionEntry* entry, const uint8_t* data, size_t length);

static void frame_copy_field_padded(const char* src, uint8_t* dst, size_t field_len) {
    if (dst == NULL || field_len == 0) {
        return;
    }
    memset(dst, 0, field_len);
    if (src == NULL) {
        return;
    }
    size_t copy_len = my_strlen(src);
    if (copy_len > field_len) {
        copy_len = field_len;
    }
    memcpy(dst, src, copy_len);
}

static void frame_extract_field(const uint8_t* src, size_t field_len, char* dst, size_t dst_len) {
    if (dst == NULL || dst_len == 0) return;
    size_t copy_len = (field_len < dst_len - 1) ? field_len : dst_len - 1;
    if (src != NULL && copy_len > 0) {
        memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
    for (ssize_t i = (ssize_t)copy_len - 1; i >= 0; --i) {
        if (dst[i] == '\0') continue;
        if (dst[i] == ' ' || dst[i] == '\0') {
            dst[i] = '\0';
        } else {
            break;
        }
    }
}

static void frame_store_field(char* dst, size_t dst_len, const char* src) {
    if (dst == NULL || dst_len == 0) return;
    size_t copy_len = 0;
    if (src != NULL) {
        copy_len = my_strlen(src);
        if (copy_len > dst_len - 1) {
            copy_len = dst_len - 1;
        }
        memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
}

void frame_init(CitadelFrame* frame, FrameType type, const char* origin, const char* destination) {
    if (frame == NULL) {
        return;
    }
    frame->type = type;
    frame->data_length = 0;
    frame->checksum = 0;
    frame->data[0] = 0;
    frame_store_field(frame->origin, sizeof(frame->origin), origin);
    frame_store_field(frame->destination, sizeof(frame->destination), destination);
}

int frame_serialize(const CitadelFrame* frame, uint8_t* buffer, size_t buffer_len, size_t* out_len) {
    if (frame == NULL || buffer == NULL) {
        return -1;
    }
    if (frame->data_length > FRAME_MAX_DATA) {
        return -1;
    }
    size_t required = FRAME_HEADER_LEN + frame->data_length + FRAME_CHECKSUM_LEN;
    if (buffer_len < required) {
        return -1;
    }

    size_t offset = 0;
    buffer[offset++] = (uint8_t)frame->type;

    frame_copy_field_padded(frame->origin, buffer + offset, FRAME_ORIGIN_LEN);
    offset += FRAME_ORIGIN_LEN;
    frame_copy_field_padded(frame->destination, buffer + offset, FRAME_DEST_LEN);
    offset += FRAME_DEST_LEN;

    buffer[offset++] = (uint8_t)((frame->data_length >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(frame->data_length & 0xFF);

    if (frame->data_length > 0) {
        memcpy(buffer + offset, frame->data, frame->data_length);
        offset += frame->data_length;
    }

    uint16_t checksum = frame_compute_checksum_bytes(buffer, offset);
    buffer[offset++] = (uint8_t)((checksum >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(checksum & 0xFF);

    if (out_len != NULL) {
        *out_len = offset;
    }
    return 0;
}

FrameParseResult frame_deserialize(const uint8_t* buffer, size_t length, CitadelFrame* frame, size_t* consumed_bytes) {
    if (consumed_bytes != NULL) {
        *consumed_bytes = 0;
    }
    if (buffer == NULL || frame == NULL) {
        return FRAME_PARSE_INVALID;
    }

    if (length < FRAME_HEADER_LEN + FRAME_CHECKSUM_LEN) {
        return FRAME_PARSE_NEED_MORE;
    }

    size_t offset = 0;
    frame->type = (FrameType)buffer[offset++];

    frame_extract_field(buffer + offset, FRAME_ORIGIN_LEN, frame->origin, sizeof(frame->origin));
    offset += FRAME_ORIGIN_LEN;
    frame_extract_field(buffer + offset, FRAME_DEST_LEN, frame->destination, sizeof(frame->destination));
    offset += FRAME_DEST_LEN;

    uint16_t data_len = (uint16_t)((buffer[offset] << 8) | buffer[offset + 1]);
    offset += 2;

    if (data_len > FRAME_MAX_DATA) {
        return FRAME_PARSE_INVALID;
    }

    size_t total_needed = FRAME_HEADER_LEN + data_len + FRAME_CHECKSUM_LEN;
    if (length < total_needed) {
        return FRAME_PARSE_NEED_MORE;
    }

    frame->data_length = data_len;
    if (data_len > 0) {
        memcpy(frame->data, buffer + offset, data_len);
    }
    offset += data_len;

    frame->checksum = (uint16_t)((buffer[offset] << 8) | buffer[offset + 1]);
    offset += 2;

    uint16_t computed = frame_compute_checksum_bytes(buffer, total_needed - FRAME_CHECKSUM_LEN);
    if (frame->checksum != computed) {
        // Debug: Log checksum mismatch
        write_str(STDERR_FILENO, "DEBUG Checksum: received=0x");
        char hex_buf[16];
        ulong_to_str(frame->checksum, hex_buf);
        write_str(STDERR_FILENO, hex_buf);
        write_str(STDERR_FILENO, " computed=0x");
        ulong_to_str(computed, hex_buf);
        write_str(STDERR_FILENO, hex_buf);
        write_str(STDERR_FILENO, " bytes_checked=");
        ulong_to_str(total_needed - FRAME_CHECKSUM_LEN, hex_buf);
        write_str(STDERR_FILENO, hex_buf);
        write_str(STDERR_FILENO, "\n");

        if (consumed_bytes != NULL) {
            *consumed_bytes = total_needed;
        }
        return FRAME_PARSE_BAD_CHECKSUM;
    }

    if (consumed_bytes != NULL) {
        *consumed_bytes = total_needed;
    }
    return FRAME_PARSE_OK;
}

uint16_t frame_compute_checksum_bytes(const uint8_t* buffer, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += buffer[i];
    }
    return (uint16_t)(sum % 65536);
}

const char* frame_type_to_string(FrameType type) {
    switch (type) {
        case FRAME_TYPE_PLEDGE: return "PLEDGE";
        case FRAME_TYPE_PLEDGE_RESPONSE: return "PLEDGE_RESP";
        case FRAME_TYPE_LIST_REQUEST: return "LIST_REQ";
        case FRAME_TYPE_LIST_RESPONSE: return "LIST_RESP";
        case FRAME_TYPE_ORDER_HEADER: return "ORDER_HDR";
        case FRAME_TYPE_ORDER_DATA: return "ORDER_DATA";
        case FRAME_TYPE_ORDER_RESPONSE: return "ORDER_RESP";
        case FRAME_TYPE_DISCONNECT: return "DISCONNECT";
        case FRAME_TYPE_ERROR_UNKNOWN: return "ERR_UNKNOWN";
        case FRAME_TYPE_ERROR_UNAUTHORIZED: return "ERR_AUTH";
        case FRAME_TYPE_ACK_FILE: return "ACK_FILE";
        case FRAME_TYPE_ACK_MD5: return "ACK_MD5";
        case FRAME_TYPE_NACK: return "NACK";
        default: return "UNKNOWN";
    }
}

void frame_log_summary(const char* prefix, const CitadelFrame* frame) {
    if (frame == NULL) return;
    if (prefix != NULL) {
        write_str(STDOUT_FILENO, prefix);
        write_str(STDOUT_FILENO, ": ");
    }
    write_str(STDOUT_FILENO, frame_type_to_string(frame->type));
    write_str(STDOUT_FILENO, " | origin=");
    write_str(STDOUT_FILENO, frame->origin);
    write_str(STDOUT_FILENO, " dest=");
    write_str(STDOUT_FILENO, frame->destination);
    write_str(STDOUT_FILENO, " len=");
    char len_buf[16];
    int_to_str(frame->data_length, len_buf);
    write_str(STDOUT_FILENO, len_buf);
    write_str(STDOUT_FILENO, "\n");
}

void frame_buffer_init(FrameBuffer* fb) {
    if (fb == NULL) return;
    fb->length = 0;
}

void frame_buffer_reset(FrameBuffer* fb) {
    if (fb == NULL) return;
    fb->length = 0;
}

static void frame_buffer_consume(FrameBuffer* fb, size_t bytes) {
    if (fb == NULL || bytes == 0 || fb->length == 0) return;
    if (bytes >= fb->length) {
        fb->length = 0;
        return;
    }
    memmove(fb->data, fb->data + bytes, fb->length - bytes);
    fb->length -= bytes;
}

int frame_buffer_append(FrameBuffer* fb, const uint8_t* data, size_t length) {
    if (fb == NULL || data == NULL) return -1;
    if (length == 0) return 0;
    if (fb->length + length > FRAME_BUFFER_CAPACITY) {
        return -1;
    }
    memcpy(fb->data + fb->length, data, length);
    fb->length += length;
    return 0;
}

FrameParseResult frame_buffer_extract(FrameBuffer* fb, CitadelFrame* frame, size_t* consumed_bytes) {
    if (fb == NULL || frame == NULL) {
        return FRAME_PARSE_INVALID;
    }
    if (fb->length == 0) {
        if (consumed_bytes) {
            *consumed_bytes = 0;
        }
        return FRAME_PARSE_NEED_MORE;
    }

    size_t consumed = 0;
    FrameParseResult result = frame_deserialize(fb->data, fb->length, frame, &consumed);
    if (result == FRAME_PARSE_OK) {
        frame_buffer_consume(fb, consumed);
        if (consumed_bytes) {
            *consumed_bytes = consumed;
        }
        return FRAME_PARSE_OK;
    }
    if (result == FRAME_PARSE_BAD_CHECKSUM || result == FRAME_PARSE_INVALID) {
        frame_buffer_reset(fb);
    }
    if (consumed_bytes) {
        *consumed_bytes = consumed;
    }
    return result;
}

static Route* maester_find_route(Maester* maester, const char* realm) {
    if (maester == NULL || realm == NULL) return NULL;
    for (int i = 0; i < maester->num_routes; i++) {
        if (my_strcasecmp(maester->routes[i].realm, realm) == 0) {
            return &maester->routes[i];
        }
    }
    return NULL;
}

static Route* maester_find_default_route(Maester* maester) {
    if (maester == NULL) return NULL;
    for (int i = 0; i < maester->num_routes; i++) {
        if (my_strcasecmp(maester->routes[i].realm, ROUTE_DEFAULT) == 0) {
            return &maester->routes[i];
        }
    }
    return NULL;
}

static int maester_route_is_known(const Route* route) {
    if (route == NULL) return 0;
    if (my_strcmp(route->ip, "*.*.*.*") == 0) {
        return 0;
    }
    return route->port > 0;
}

Route* maester_resolve_route(Maester* maester, const char* destination, int* used_default) {
    if (used_default != NULL) {
        *used_default = 0;
    }
    Route* direct = maester_find_route(maester, destination);
    if (direct != NULL && maester_route_is_known(direct)) {
        return direct;
    }
    Route* def = maester_find_default_route(maester);
    if (def != NULL && maester_route_is_known(def)) {
        if (used_default != NULL) {
            *used_default = 1;
        }
        return def;
    }
    return NULL;
}

void maester_log_route_resolution(const char* destination, const Route* route, int used_default) {
    if (destination == NULL || route == NULL) return;
    write_str(STDOUT_FILENO, "Routing ");
    write_str(STDOUT_FILENO, destination);
    write_str(STDOUT_FILENO, used_default ? " via DEFAULT hop " : " directly ");
    write_str(STDOUT_FILENO, "through ");
    write_str(STDOUT_FILENO, route->realm);
    write_str(STDOUT_FILENO, " (");
    write_str(STDOUT_FILENO, route->ip);
    write_str(STDOUT_FILENO, ":");
    char buf[16];
    int_to_str(route->port, buf);
    write_str(STDOUT_FILENO, buf);
    write_str(STDOUT_FILENO, ").\n");
}

static ConnectionEntry* maester_find_connection(Maester* maester, const char* realm) {
    if (maester == NULL || realm == NULL) return NULL;
    for (int i = 0; i < maester->num_connections; i++) {
        if (my_strcasecmp(maester->connections[i].peer_realm, realm) == 0) {
            return &maester->connections[i];
        }
    }
    return NULL;
}

ConnectionEntry* maester_add_connection_entry(Maester* maester) {
    if (maester == NULL) return NULL;
    if (maester->num_connections >= maester->connections_capacity) {
        int new_capacity = (maester->connections_capacity == 0) ? 4 : maester->connections_capacity * 2;
        ConnectionEntry* new_entries = (ConnectionEntry*)realloc(maester->connections, new_capacity * sizeof(ConnectionEntry));
        if (new_entries == NULL) {
            write_str(STDERR_FILENO, "Error: Unable to expand connection table.\n");
            return NULL;
        }
        maester->connections = new_entries;
        maester->connections_capacity = new_capacity;
    }
    ConnectionEntry* entry = &maester->connections[maester->num_connections++];
    memset(entry, 0, sizeof(ConnectionEntry));
    entry->sockfd = -1;
    frame_buffer_init(&entry->recv_buffer);
    frame_buffer_init(&entry->send_buffer);
    return entry;
}

void maester_close_connection_entry(ConnectionEntry* entry) {
    if (entry == NULL) return;
    if (entry->sockfd >= 0) {
        close(entry->sockfd);
        entry->sockfd = -1;
    }
    entry->peer_realm[0] = '\0';
    entry->peer_ip[0] = '\0';
    entry->peer_port = 0;
    entry->last_used = 0;
    memset(&entry->addr, 0, sizeof(entry->addr));
    frame_buffer_reset(&entry->recv_buffer);
    frame_buffer_reset(&entry->send_buffer);
}

static int set_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

ConnectionEntry* maester_get_or_open_connection(Maester* maester, const char* realm, const char* ip, int port) {
    if (maester == NULL || realm == NULL || ip == NULL || port <= 0) {
        return NULL;
    }

    ConnectionEntry* existing = maester_find_connection(maester, realm);
    if (existing != NULL && existing->sockfd >= 0) {
        existing->last_used = time(NULL);
        return existing;
    }

    ConnectionEntry* entry = maester_add_connection_entry(maester);
    if (entry == NULL) {
        return NULL;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        write_str(STDERR_FILENO, "Error: Unable to create client socket.\n");
        maester_close_connection_entry(entry);
        maester->num_connections--;
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        write_str(STDERR_FILENO, "Error: Invalid IP when opening connection.\n");
        close(fd);
        maester_close_connection_entry(entry);
        maester->num_connections--;
        return NULL;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        write_str(STDERR_FILENO, "Error: connect() failed when reaching ");
        write_str(STDERR_FILENO, realm);
        write_str(STDERR_FILENO, ".\n");
        close(fd);
        maester_close_connection_entry(entry);
        maester->num_connections--;
        return NULL;
    }

    if (set_socket_nonblocking(fd) < 0) {
        write_str(STDERR_FILENO, "Warning: failed to set non-blocking mode on connection socket.\n");
    }

    entry->sockfd = fd;
    entry->addr = addr;
    my_strcpy(entry->peer_realm, realm);
    my_strcpy(entry->peer_ip, ip);
    entry->peer_port = port;
    entry->last_used = time(NULL);
    char port_buf[16];
    int_to_str(port, port_buf);
    write_str(STDOUT_FILENO, "Connected to ");
    write_str(STDOUT_FILENO, realm);
    write_str(STDOUT_FILENO, " (");
    write_str(STDOUT_FILENO, ip);
    write_str(STDOUT_FILENO, ":");
    write_str(STDOUT_FILENO, port_buf);
    write_str(STDOUT_FILENO, ").\n");
    return entry;
}

void maester_compact_connections(Maester* maester) {
    if (maester == NULL || maester->connections == NULL) return;
    int write_idx = 0;
    for (int i = 0; i < maester->num_connections; i++) {
        if (maester->connections[i].sockfd >= 0) {
            if (write_idx != i) {
                maester->connections[write_idx] = maester->connections[i];
            }
            write_idx++;
        }
    }
    maester->num_connections = write_idx;
}

void maester_close_all_connections(Maester* maester) {
    if (maester == NULL || maester->connections == NULL) return;
    for (int i = 0; i < maester->num_connections; i++) {
        maester_close_connection_entry(&maester->connections[i]);
    }
    maester->num_connections = 0;
}

static int maester_queue_bytes(ConnectionEntry* entry, const uint8_t* data, size_t length) {
    if (entry == NULL || entry->sockfd < 0 || data == NULL || length == 0) {
        return -1;
    }

    if (entry->send_buffer.length > 0) {
        return frame_buffer_append(&entry->send_buffer, data, length);
    }

    ssize_t sent = send(entry->sockfd, data, length, 0);
    if (sent == (ssize_t)length) {
        return 0;
    }
    if (sent > 0) {
        size_t remaining = length - (size_t)sent;
        return frame_buffer_append(&entry->send_buffer, data + sent, remaining);
    }
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return frame_buffer_append(&entry->send_buffer, data, length);
    }
    return -1;
}

int maester_connection_has_pending_send(const ConnectionEntry* entry) {
    if (entry == NULL) return 0;
    return entry->send_buffer.length > 0;
}

void maester_flush_send_buffer(ConnectionEntry* entry) {
    if (entry == NULL || entry->sockfd < 0) {
        return;
    }
    while (entry->send_buffer.length > 0) {
        ssize_t sent = send(entry->sockfd, entry->send_buffer.data, entry->send_buffer.length, 0);
        if (sent > 0) {
            frame_buffer_consume(&entry->send_buffer, (size_t)sent);
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        maester_close_connection_entry(entry);
        break;
    }
}

int maester_send_frame(ConnectionEntry* entry, const CitadelFrame* frame) {
    if (entry == NULL || frame == NULL) {
        return -1;
    }
    uint8_t buffer[FRAME_MAX_SIZE];
    size_t length = 0;
    if (frame_serialize(frame, buffer, sizeof(buffer), &length) != 0) {
        return -1;
    }
    return maester_queue_bytes(entry, buffer, length);
}
