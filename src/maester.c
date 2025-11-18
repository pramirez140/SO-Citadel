#include "maester.h"
#include "trade.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 256

static void maester_reset_active_mission(Maester* maester);
static int  set_socket_nonblocking(int fd);
static int  maester_setup_listener(Maester* maester);
static void maester_accept_placeholder(Maester* maester);
static void maester_event_loop(Maester* maester);
static void frame_copy_field_padded(const char* src, uint8_t* dst, size_t field_len);
static void frame_extract_field(const uint8_t* src, size_t field_len, char* dst, size_t dst_len);
static void frame_buffer_consume(FrameBuffer* fb, size_t bytes);
static void frame_store_field(char* dst, size_t dst_len, const char* src);

Maester* create_maester(const char* realm_name, const char* folder_path, const char* ip, int port) {
    Maester* maester = (Maester*)malloc(sizeof(Maester));
    if (maester == NULL) {
        return NULL;
    }

    // Initialize fields
    my_strcpy(maester->realm_name, realm_name);
    clean_realm_name(maester->realm_name);
    my_strcpy(maester->folder_path, folder_path);
    my_strcpy(maester->ip, ip);
    maester->port = port;
    if (maester->port < 8685 || maester->port > 8694) {
        write_str(STDERR_FILENO, "Warning: Port not in assigned range (8685-8694)\n");
    }
    maester->socket_fd = -1; 
    maester->num_envoys = 0;
    maester->routes = NULL;
    maester->num_routes = 0;
    maester->stock = NULL;
    maester->num_products = 0;
    maester->alliances = NULL;
    maester->num_alliances = 0;
    maester->envoy_missions = NULL;
    maester->connections = NULL;
    maester->num_connections = 0;
    maester->listen_fd = -1;
    maester->listener_thread = 0;
    maester->shutting_down = 0;
    maester->outbound_queue.buffer = NULL;
    maester->outbound_queue.capacity = 0;
    maester->outbound_queue.count = 0;
    maester->outbound_queue.head = 0;
    maester->outbound_queue.tail = 0;
    pthread_mutex_init(&maester->routes_lock, NULL);
    pthread_mutex_init(&maester->alliances_lock, NULL);
    pthread_mutex_init(&maester->envoys_lock, NULL);
    pthread_mutex_init(&maester->connections_lock, NULL);
    pthread_mutex_init(&maester->outbound_queue.lock, NULL);
    pthread_cond_init(&maester->outbound_queue.cond, NULL);
    maester_reset_active_mission(maester);

    return maester;
}

void add_route(Maester* maester, const char* realm, const char* ip, int port) {
    if (maester == NULL) return;

    // Reallocate routes array
    Route* new_routes = (Route*)realloc(maester->routes,
                                        (maester->num_routes + 1) * sizeof(Route));
    if (new_routes == NULL) {
        write_str(STDERR_FILENO, "Error: Failed to allocate memory for route\n");
        return;
    }

    maester->routes = new_routes;

    // Add new route
    my_strcpy(maester->routes[maester->num_routes].realm, realm);
    clean_realm_name(maester->routes[maester->num_routes].realm);
    my_strcpy(maester->routes[maester->num_routes].ip, ip);
    maester->routes[maester->num_routes].port = port;

    maester->num_routes++;
}

void add_product(Maester* maester, const char* name, float weight, int quantity) {
    if (maester == NULL) return;

    // Reallocate products array
    Product* new_stock = (Product*)realloc(maester->stock,
                                          (maester->num_products + 1) * sizeof(Product));
    if (new_stock == NULL) {
        write_str(STDERR_FILENO, "Error: Failed to allocate memory for product\n");
        return;
    }

    maester->stock = new_stock;

    // Add new product
    my_strcpy(maester->stock[maester->num_products].name, name);
    maester->stock[maester->num_products].weight = weight;
    maester->stock[maester->num_products].amount = quantity;

    maester->num_products++;
}

Maester* load_maester_config(const char* config_file, const char* stock_file) {
    int fd = open(config_file, O_RDONLY);
    if (fd < 0) {
        write_str(STDERR_FILENO, "Error: Cannot open config file: ");
        write_str(STDERR_FILENO, config_file);
        write_str(STDERR_FILENO, "\n");
        return NULL;
    }

    char line[MAX_LINE_LENGTH];
    char realm_name[REALM_NAME_MAX];
    char folder_path[PATH_MAX_LEN];
    char ip[IP_ADDR_MAX];
    int num_envoys = 0;
    int port = 0;

    // Read realm name
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(realm_name, line);

    // Read folder path
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(folder_path, line);

    // Read number of envoys
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    num_envoys = str_to_int(line);

    // Read IP
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(ip, line);

    // Read port
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    port = str_to_int(line);

    // Create maester
    Maester* maester = create_maester(realm_name, folder_path, ip, port);
    if (maester == NULL) {
        close(fd);
        return NULL;
    }
    maester->num_envoys = num_envoys;
    if (maester->num_envoys > 0) {
        maester->envoy_missions = (EnvoyMission*)calloc(maester->num_envoys, sizeof(EnvoyMission));
        if (maester->envoy_missions == NULL) {
            close(fd);
            free_maester(maester);
            return NULL;
        }
        for (int i = 0; i < maester->num_envoys; i++) {
            maester->envoy_missions[i].type = ENVOY_TYPE_NONE;
            maester->envoy_missions[i].status = ENVOY_STATUS_IDLE;
            maester->envoy_missions[i].target_realm[0] = '\0';
            maester->envoy_missions[i].payload_path[0] = '\0';
            maester->envoy_missions[i].started_at = 0;
        }
    }

    // Create folder if it doesn't exist (mkdir with 0755 permissions)
    mkdir(folder_path, 0755);  // Ignore error if folder already exists

    // Read "--- ROUTES ---" marker
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        free_maester(maester);
        return NULL;
    }
    if (my_strcasecmp(line, "--- ROUTES ---") != 0) {
        write_str(STDERR_FILENO, "Warning: Expected '--- ROUTES ---' section\n");
    }

    // Read routes until EOF
    while (1) {
        int len = read_line_fd(fd, line, MAX_LINE_LENGTH);
        if (len <= 0) {
            break;
        }
        if (line[0] == '\0') {
            continue;
        }

        char token[3][REALM_NAME_MAX];
        int token_count = 0;
        int i = 0;

        while (line[i] != '\0' && token_count < 3) {
            while (line[i] == ' ' || line[i] == '\t') {
                i++;
            }
            if (line[i] == '\0') {
                break;
            }
            int j = 0;
            while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t') {
                token[token_count][j++] = line[i++];
            }
            token[token_count][j] = '\0';
            token_count++;
        }

        if (token_count < 3) {
            continue;
        }

        int route_port = str_to_int(token[2]);
        add_route(maester, token[0], token[1], route_port);
    }

    close(fd);

    // Load stock database
    int num_products = 0;
    Product* stock = load_stock(stock_file, &num_products);
    if (stock != NULL) {
        maester->stock = stock;
        maester->num_products = num_products;
    }

    return maester;
}

void free_maester(Maester* maester) {
    if (maester == NULL) return;

    if (maester->listen_fd >= 0) {
        close(maester->listen_fd);
        maester->listen_fd = -1;
    }
    if (maester->socket_fd >= 0) {
        close(maester->socket_fd);
        maester->socket_fd = -1;
    }

    if (maester->alliances != NULL) {
        free(maester->alliances);
    }

    if (maester->envoy_missions != NULL) {
        free(maester->envoy_missions);
    }

    if (maester->connections != NULL) {
        free(maester->connections);
    }

    if (maester->routes != NULL) {
        free(maester->routes);
    }

    if (maester->stock != NULL) {
        free(maester->stock);
    }

    if (maester->outbound_queue.buffer != NULL) {
        free(maester->outbound_queue.buffer);
    }

    pthread_mutex_destroy(&maester->routes_lock);
    pthread_mutex_destroy(&maester->alliances_lock);
    pthread_mutex_destroy(&maester->envoys_lock);
    pthread_mutex_destroy(&maester->connections_lock);
    pthread_mutex_destroy(&maester->outbound_queue.lock);
    pthread_cond_destroy(&maester->outbound_queue.cond);

    free(maester);
}

static void maester_reset_active_mission(Maester* maester) {
    if (maester == NULL) {
        return;
    }
    maester->active_mission.in_use = 0;
    maester->active_mission.type = FRAME_TYPE_PLEDGE;
    maester->active_mission.target_realm[0] = '\0';
    maester->active_mission.deadline = 0;
}

// ============= COMMAND HANDLERS PHASE 1 =============

void cmd_list_realms(Maester* maester) {
    if (maester == NULL) return;

    write_str(STDOUT_FILENO, "Known Realms:\n");
    int printed = 0;
    for (int i = 0; i < maester->num_routes; i++) {
        // Skip DEFAULT - it's not a realm, it's the default route
        if (my_strcasecmp(maester->routes[i].realm, ROUTE_DEFAULT) == 0) {
            continue;
        }

        write_str(STDOUT_FILENO, "  - ");
        write_str(STDOUT_FILENO, maester->routes[i].realm);

        // Indicate if route is known or unknown
        if (my_strcmp(maester->routes[i].ip, "*.*.*.*") == 0) {
            write_str(STDOUT_FILENO, " (route unknown)");
        }
        write_str(STDOUT_FILENO, "\n");
        printed = 1;
    }
    if (!printed) {
        write_str(STDOUT_FILENO, "  (none)\n");
    }
}

void cmd_list_products(Maester* maester, const char* realm) {
    if (maester == NULL) return;

    if (realm == NULL || my_strlen(realm) == 0) {
        print_products(maester->num_products, maester->stock);
        return;
    }

    write_str(STDOUT_FILENO, "ERROR: You must have an alliance with ");
    write_str(STDOUT_FILENO, realm);
    write_str(STDOUT_FILENO, " to trade.\n");
}

void cmd_pledge(Maester* maester, const char* realm, const char* sigil) {
    if (maester == NULL || realm == NULL) return;

    (void)sigil;
    (void)realm;

    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_pledge_respond(Maester* maester, const char* realm, const char* response) {
    if (maester == NULL || realm == NULL || response == NULL) return;

    (void)realm;
    (void)response;

    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_pledge_status(Maester* maester) {
    if (maester == NULL) return;

    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_envoy_status(Maester* maester) {
    if (maester == NULL) return;

    write_str(STDOUT_FILENO, "Command OK\n");
}

// ========== Frame utilities (Phase 2) ==========

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
    // Trim trailing nulls/spaces
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

uint16_t frame_compute_checksum_bytes(const uint8_t* buffer, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += buffer[i];
    }
    return (uint16_t)(sum % 65536);
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


// ========== Module-local state for CTRL+C ==========
static volatile sig_atomic_t g_should_exit = 0;

static void maester_handle_sigint(int sig) {
    (void)sig;
    g_should_exit = 1;
    // write() is async-signal-safe
    write(STDOUT_FILENO, "\nReceived SIGINT. Shutting down gracefully...\n", 46);
}

// ========== Small tokenizer used by the CLI ==========
static int parse_command(char* input, char* tokens[], int max_tokens) {
    int count = 0, i = 0, in_token = 0;
    while (input[i] != '\0' && count < max_tokens) {
        if (input[i] == ' ' || input[i] == '\t') {
            if (in_token) { input[i] = '\0'; in_token = 0; }
        } else {
            if (!in_token) { tokens[count++] = &input[i]; in_token = 1; }
        }
        i++;
    }
    return count;
}

// ========== Dispatch one command ==========
static void process_command(Maester* maester, char* input) {
    char* tokens[10];
    int token_count = parse_command(input, tokens, 10);
    if (token_count == 0) return;

    // LIST ...
    if (my_strcasecmp(tokens[0], "LIST") == 0) {
        if (token_count == 1) {
            write_str(STDOUT_FILENO, "Did you mean to list something? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: LIST REALMS or LIST PRODUCTS [<REALM>]\n");
            return;
        }
        if (my_strcasecmp(tokens[1], "REALMS") == 0) { cmd_list_realms(maester); return; }
        if (my_strcasecmp(tokens[1], "PRODUCTS") == 0) {
            if (token_count == 2) { cmd_list_products(maester, NULL); }
            else { cmd_list_products(maester, tokens[2]); }
            return;
        }
        write_str(STDOUT_FILENO, "Unknown LIST command. Try: LIST REALMS or LIST PRODUCTS\n");
        return;
    }

    // PLEDGE ...
    if (my_strcasecmp(tokens[0], "PLEDGE") == 0) {
        if (token_count >= 2 && my_strcasecmp(tokens[1], "RESPOND") == 0) {
            if (token_count >= 4) { cmd_pledge_respond(maester, tokens[2], tokens[3]); }
            else {
                write_str(STDOUT_FILENO, "Did you mean to respond to a pledge? Please review syntax.\n");
                write_str(STDOUT_FILENO, "Usage: PLEDGE RESPOND <REALM> ACCEPT|REJECT\n");
            }
        } else if (token_count >= 2 && my_strcasecmp(tokens[1], "STATUS") == 0) {
            cmd_pledge_status(maester);
        } else if (token_count >= 3) {
            cmd_pledge(maester, tokens[1], tokens[2]);
        } else {
            write_str(STDOUT_FILENO, "Did you mean to send a pledge? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: PLEDGE <REALM> <sigil.jpg>\n");
        }
        return;
    }

    // START TRADE ...
    if (my_strcasecmp(tokens[0], "START") == 0) {
        if (token_count == 1) {
            write_str(STDOUT_FILENO, "Did you mean to start something? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: START TRADE <REALM>\n");
            return;
        }
        if (my_strcasecmp(tokens[1], "TRADE") == 0) {
            if (token_count >= 3) { cmd_start_trade(maester, tokens[2]); }
            else {
                write_str(STDOUT_FILENO, "Did you mean to start a trade? Please review syntax.\n");
                write_str(STDOUT_FILENO, "Usage: START TRADE <REALM>\n");
            }
            return;
        }
        write_str(STDOUT_FILENO, "Unknown START command. Try: START TRADE <REALM>\n");
        return;
    }

    // ENVOY STATUS
    if (my_strcasecmp(tokens[0], "ENVOY") == 0) {
        if (token_count >= 2 && my_strcasecmp(tokens[1], "STATUS") == 0) { cmd_envoy_status(maester); }
        else {
            write_str(STDOUT_FILENO, "Did you mean to check envoy status? Please review syntax.\n");
            write_str(STDOUT_FILENO, "Usage: ENVOY STATUS\n");
        }
        return;
    }

    // EXIT
    if (token_count == 1 && my_strcasecmp(tokens[0], "EXIT") == 0) {
        g_should_exit = 1;
        maester->shutting_down = 1;
        return;
    }

    write_str(STDOUT_FILENO, "Unknown command\n");
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

static int maester_setup_listener(Maester* maester) {
    if (maester == NULL) {
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        write_str(STDERR_FILENO, "Error: Unable to create listening socket.\n");
        return -1;
    }

    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        write_str(STDERR_FILENO, "Warning: setsockopt(SO_REUSEADDR) failed.\n");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)maester->port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        write_str(STDERR_FILENO, "Error: bind() failed. Is the port already in use?\n");
        close(fd);
        return -1;
    }

    if (set_socket_nonblocking(fd) < 0) {
        write_str(STDERR_FILENO, "Error: failed to set listener socket non-blocking.\n");
        close(fd);
        return -1;
    }

    int backlog = maester->num_envoys + 4;
    if (backlog < 4) backlog = 4;
    if (listen(fd, backlog) < 0) {
        write_str(STDERR_FILENO, "Error: listen() failed on listener socket.\n");
        close(fd);
        return -1;
    }

    maester->listen_fd = fd;

    char port_buffer[16];
    int_to_str(maester->port, port_buffer);
    write_str(STDOUT_FILENO, "Listening for ravens on port ");
    write_str(STDOUT_FILENO, port_buffer);
    write_str(STDOUT_FILENO, " (INADDR_ANY).\n");
    return 0;
}

static void maester_accept_placeholder(Maester* maester) {
    if (maester == NULL || maester->listen_fd < 0) {
        return;
    }

    while (1) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int client_fd = accept(maester->listen_fd, (struct sockaddr*)&addr, &addrlen);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            write_str(STDERR_FILENO, "Warning: accept() failed on listener socket.\n");
            break;
        }

        write_str(STDOUT_FILENO, "\nIncoming connection received (Phase 2 networking placeholder).\n");
        close(client_fd);
    }
}

static void maester_event_loop(Maester* maester) {
    if (maester == NULL) return;

    struct pollfd pollfds[2];
    int need_prompt = 1;
    char input[256];

    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN;
    pollfds[0].revents = 0;

    while (!g_should_exit && !maester->shutting_down) {
        if (need_prompt) {
            write_str(STDOUT_FILENO, "$ ");
            need_prompt = 0;
        }

        int nfds = 1;
        if (maester->listen_fd >= 0) {
            pollfds[1].fd = maester->listen_fd;
            pollfds[1].events = POLLIN;
            pollfds[1].revents = 0;
            nfds = 2;
        }

        int poll_result = poll(pollfds, nfds, 500);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            write_str(STDERR_FILENO, "Poll failed. Leaving event loop.\n");
            break;
        }
        if (poll_result == 0) {
            continue;
        }

        if (pollfds[0].revents & (POLLIN | POLLPRI)) {
            int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    write_str(STDOUT_FILENO, "\nStandard input closed. Exiting...\n");
                } else if (errno != EINTR) {
                    write_str(STDERR_FILENO, "Error reading from stdin. Exiting...\n");
                }
                g_should_exit = 1;
            } else {
                input[bytes_read] = '\0';
                for (int i = 0; i < bytes_read; i++) {
                    if (input[i] == '\n' || input[i] == '\r') {
                        input[i] = '\0';
                        break;
                    }
                }
                if (my_strlen(input) > 0) {
                    process_command(maester, input);
                }
            }
            need_prompt = 1;
        } else if (pollfds[0].revents & (POLLHUP | POLLERR)) {
            g_should_exit = 1;
        }

        if (nfds == 2 && (pollfds[1].revents & POLLIN)) {
            maester_accept_placeholder(maester);
            need_prompt = 1;
        } else if (nfds == 2 && (pollfds[1].revents & (POLLHUP | POLLERR))) {
            write_str(STDERR_FILENO, "Listener socket reported an error.\n");
        }
    }

    maester->shutting_down = 1;
}

int maester_run(const char* config_file, const char* stock_file) {
    // Setup CTRL+C handling for the CLI loop
    signal(SIGINT, maester_handle_sigint);

    Maester* maester = load_maester_config(config_file, stock_file);
    if (maester == NULL) {
        die("Error: Could not load maester configuration\n");
    }

    write_str(STDOUT_FILENO, "Maester of ");
    write_str(STDOUT_FILENO, maester->realm_name);
    write_str(STDOUT_FILENO, " initialized. The board is set.\n\n");

    if (maester_setup_listener(maester) < 0) {
        free_maester(maester);
        die("Unable to initialize networking listener.\n");
    }

    maester_event_loop(maester);

    write_str(STDOUT_FILENO, "\nCleaning up resources...\n");
    free_maester(maester);
    write_str(STDOUT_FILENO, "Maester process terminated. Farewell.\n");
    return 0;
}
