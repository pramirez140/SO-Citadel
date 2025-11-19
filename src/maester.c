#include "maester.h"
#include "trade.h"
#include "network.h"
#include "missions.h"

#define MAX_LINE_LENGTH 256

static int  set_socket_nonblocking(int fd);
static int  maester_setup_listener(Maester* maester);
static void maester_accept_placeholder(Maester* maester);
static void maester_event_loop(Maester* maester);
static int  build_origin_string(const Maester* maester, char* buffer, size_t len);
static int  maester_join_path(const char* base, const char* relative, char* output, size_t output_len);
static const char* maester_basename(const char* path);
static int  maester_prepare_sigil_metadata(Maester* maester, const char* sigil, char* sigil_name,
                                           size_t sigil_name_len, char* file_size_str,
                                           size_t size_len, char md5_hex[33]);
static void   maester_handle_connection_event(Maester* maester, ConnectionEntry* entry, short revents);
static void   maester_receive_placeholder(Maester* maester, ConnectionEntry* entry);
static void   maester_process_incoming_frame(Maester* maester, ConnectionEntry* entry, const CitadelFrame* frame);

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
    maester->connections_capacity = 0;
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
    maester_mission_init(maester);

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

    maester_close_all_connections(maester);
    if (maester->connections != NULL) {
        free(maester->connections);
        maester->connections = NULL;
        maester->connections_capacity = 0;
    }

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

static int build_origin_string(const Maester* maester, char* buffer, size_t len) {
    if (maester == NULL || buffer == NULL || len == 0) return -1;
    int written = snprintf(buffer, len, "%s:%d", maester->ip, maester->port);
    if (written < 0 || (size_t)written >= len || (size_t)written > FRAME_ORIGIN_LEN) {
        return -1;
    }
    return 0;
}

static int maester_join_path(const char* base, const char* relative, char* output, size_t output_len) {
    if (output == NULL || relative == NULL || output_len == 0) return -1;

    if (relative[0] == '/' || relative[0] == '\\') {
        size_t rel_len = my_strlen(relative);
        if (rel_len + 1 > output_len) return -1;
        my_strcpy(output, relative);
        return 0;
    }

    if (base == NULL || base[0] == '\0') {
        size_t rel_len = my_strlen(relative);
        if (rel_len + 1 > output_len) return -1;
        my_strcpy(output, relative);
        return 0;
    }

    output[0] = '\0';
    if (safe_append(output, output_len, base) != 0) return -1;
    size_t base_len = my_strlen(output);
    if (base_len > 0 && output[base_len - 1] != '/') {
        if (safe_append(output, output_len, "/") != 0) return -1;
    }
    if (safe_append(output, output_len, relative) != 0) return -1;
    return 0;
}

static const char* maester_basename(const char* path) {
    if (path == NULL) return "";
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static int maester_prepare_sigil_metadata(Maester* maester, const char* sigil, char* sigil_name,
                                          size_t sigil_name_len, char* file_size_str,
                                          size_t size_len, char md5_hex[33]) {
    if (maester == NULL || sigil == NULL || sigil_name == NULL || file_size_str == NULL || md5_hex == NULL) {
        return -1;
    }

    char full_path[PATH_MAX_LEN * 2];
    if (maester_join_path(maester->folder_path, sigil, full_path, sizeof(full_path)) != 0) {
        write_str(STDOUT_FILENO, "Error: Sigil path too long.\n");
        return -1;
    }

    struct stat st;
    if (stat(full_path, &st) < 0 || !S_ISREG(st.st_mode)) {
        write_str(STDOUT_FILENO, "Error: Sigil file not found: ");
        write_str(STDOUT_FILENO, full_path);
        write_str(STDOUT_FILENO, "\n");
        return -1;
    }

    uint8_t digest[16];
    if (md5_compute_file(full_path, digest) != 0) {
        write_str(STDOUT_FILENO, "Error: Could not compute MD5 of sigil file.\n");
        return -1;
    }

    md5_digest_to_hex(digest, md5_hex);

    const char* base_name = maester_basename(sigil);
    size_t base_len = my_strlen(base_name);
    if (base_len + 1 > sigil_name_len) {
        write_str(STDOUT_FILENO, "Error: Sigil file name too long.\n");
        return -1;
    }
    my_strcpy(sigil_name, base_name);

    char size_buffer[32];
    ulong_to_str((unsigned long long)st.st_size, size_buffer);
    size_t size_buffer_len = my_strlen(size_buffer);
    if (size_buffer_len + 1 > size_len) {
        write_str(STDOUT_FILENO, "Error: Could not convert file size.\n");
        return -1;
    }
    my_strcpy(file_size_str, size_buffer);

    return 0;
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

    if (maester_mission_is_active(maester)) {
        maester_mission_print_busy(maester, "a product list request");
        return;
    }

    write_str(STDOUT_FILENO, "ERROR: You must have an alliance with ");
    write_str(STDOUT_FILENO, realm);
    write_str(STDOUT_FILENO, " to trade.\n");
}

void cmd_pledge(Maester* maester, const char* realm, const char* sigil) {
    if (maester == NULL || realm == NULL) return;

    if (maester_mission_is_active(maester)) {
        maester_mission_print_busy(maester, "a pledge mission");
        return;
    }

    char sigil_name[PATH_MAX_LEN];
    char file_size_str[32];
    char md5_hex[33];
    if (maester_prepare_sigil_metadata(maester, sigil, sigil_name, sizeof(sigil_name),
                                       file_size_str, sizeof(file_size_str), md5_hex) != 0) {
        return;
    }

    char origin[FRAME_ORIGIN_LEN + 1];
    if (build_origin_string(maester, origin, sizeof(origin)) != 0) {
        write_str(STDOUT_FILENO, "Error: Origin endpoint too long.\n");
        return;
    }

    CitadelFrame frame;
    frame_init(&frame, FRAME_TYPE_PLEDGE, origin, realm);
    int payload_len = snprintf((char*)frame.data, FRAME_MAX_DATA, "%s&%s&%s&%s",
                               maester->realm_name, sigil_name, file_size_str, md5_hex);
    if (payload_len < 0 || payload_len >= FRAME_MAX_DATA) {
        write_str(STDOUT_FILENO, "Error: Pledge data too large.\n");
        return;
    }
    frame.data_length = (uint16_t)payload_len;

    int used_default = 0;
    Route* route = maester_resolve_route(maester, realm, &used_default);
    if (route == NULL) {
        write_str(STDOUT_FILENO, "Unable to find a valid route to ");
        write_str(STDOUT_FILENO, realm);
        write_str(STDOUT_FILENO, ".\n");
        return;
    }

    maester_log_route_resolution(realm, route, used_default);

    ConnectionEntry* connection = maester_get_or_open_connection(maester, route->realm, route->ip, route->port);
    if (connection == NULL) {
        write_str(STDOUT_FILENO, "Failed to prepare connection for pledge.\n");
        return;
    }

    if (maester_send_frame(connection, &frame) != 0) {
        write_str(STDOUT_FILENO, "Error: Failed to send pledge frame.\n");
        maester_close_connection_entry(connection);
        return;
    }

    char mission_desc[64];
    mission_desc[0] = '\0';
    safe_append(mission_desc, sizeof(mission_desc), "Pledge to ");
    safe_append(mission_desc, sizeof(mission_desc), realm);
    if (!maester_mission_begin(maester, FRAME_TYPE_PLEDGE, realm, mission_desc, 120)) {
        return;
    }

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

    maester_mission_check_timeouts(maester);
    if (!maester_mission_is_active(maester)) {
        write_str(STDOUT_FILENO, "No active missions.\n");
        return;
    }

    write_str(STDOUT_FILENO, "Mission in progress: ");
    if (maester->active_mission.description[0] != '\0') {
        write_str(STDOUT_FILENO, maester->active_mission.description);
    } else {
        write_str(STDOUT_FILENO, frame_type_to_string(maester->active_mission.type));
    }
    if (maester->active_mission.target_realm[0] != '\0') {
        write_str(STDOUT_FILENO, " (target: ");
        write_str(STDOUT_FILENO, maester->active_mission.target_realm);
        write_str(STDOUT_FILENO, ")");
    }
    write_str(STDOUT_FILENO, ".\n");

    if (maester->active_mission.deadline > 0) {
        time_t now = time(NULL);
        long remaining = (long)(maester->active_mission.deadline - now);
        if (remaining < 0) remaining = 0;
        write_str(STDOUT_FILENO, "Time remaining: ");
        char buf[16];
        int_to_str((int)remaining, buf);
        write_str(STDOUT_FILENO, buf);
        write_str(STDOUT_FILENO, " seconds.\n");
    }
}

void cmd_envoy_status(Maester* maester) {
    if (maester == NULL) return;

    write_str(STDOUT_FILENO, "Command OK\n");
}

static void maester_receive_placeholder(Maester* maester, ConnectionEntry* entry) {
    if (entry == NULL || entry->sockfd < 0) return;
    uint8_t buffer[FRAME_MAX_SIZE];
    ssize_t bytes = read(entry->sockfd, buffer, sizeof(buffer));
    if (bytes > 0) {
        // Log received bytes
        write_str(STDOUT_FILENO, "DEBUG: Received ");
        char bytes_buf[32];
        long_to_str(bytes, bytes_buf);
        write_str(STDOUT_FILENO, bytes_buf);
        write_str(STDOUT_FILENO, " bytes from peer ");
        write_str(STDOUT_FILENO, entry->peer_ip);
        write_str(STDOUT_FILENO, "\n");

        if (frame_buffer_append(&entry->recv_buffer, buffer, (size_t)bytes) != 0) {
            write_str(STDERR_FILENO, "Warning: receive buffer overflow, dropping data.\n");
            frame_buffer_reset(&entry->recv_buffer);
            return;
        }
        CitadelFrame frame;
        size_t consumed = 0;
        FrameParseResult result;
        int frame_count = 0;
        while ((result = frame_buffer_extract(&entry->recv_buffer, &frame, &consumed)) == FRAME_PARSE_OK) {
            frame_count++;
            maester_process_incoming_frame(maester, entry, &frame);
        }

        // Log parse results
        if (frame_count > 0) {
            write_str(STDOUT_FILENO, "DEBUG: Successfully parsed ");
            char count_buf[16];
            int_to_str(frame_count, count_buf);
            write_str(STDOUT_FILENO, count_buf);
            write_str(STDOUT_FILENO, " frame(s)\n");
        } else if (result == FRAME_PARSE_NEED_MORE) {
            write_str(STDOUT_FILENO, "DEBUG: Need more data for complete frame\n");
        } else if (result == FRAME_PARSE_BAD_CHECKSUM) {
            write_str(STDERR_FILENO, "Warning: Received frame with invalid checksum.\n");
        } else if (result == FRAME_PARSE_INVALID) {
            write_str(STDERR_FILENO, "Warning: Invalid frame format.\n");
        }
    } else if (bytes == 0) {
        write_str(STDOUT_FILENO, "Peer closed connection: ");
        write_str(STDOUT_FILENO, entry->peer_realm[0] ? entry->peer_realm : entry->peer_ip);
        write_str(STDOUT_FILENO, "\n");
        maester_close_connection_entry(entry);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            write_str(STDERR_FILENO, "Error reading from peer connection. Closing it.\n");
            maester_close_connection_entry(entry);
        }
    }
}

static void maester_handle_connection_event(Maester* maester, ConnectionEntry* entry, short revents) {
    if (entry == NULL) return;
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        write_str(STDERR_FILENO, "Connection error with peer ");
        write_str(STDERR_FILENO, entry->peer_realm);
        write_str(STDERR_FILENO, ". Closing.\n");
        maester_close_connection_entry(entry);
        return;
    }
    if ((revents & POLLOUT) && entry->sockfd >= 0) {
        maester_flush_send_buffer(entry);
        if (entry->sockfd < 0) {
            return;
        }
    }
    if (revents & POLLIN) {
        maester_receive_placeholder(maester, entry);
    }
}

static void maester_process_incoming_frame(Maester* maester, ConnectionEntry* entry, const CitadelFrame* frame) {
    (void)entry;
    frame_log_summary("Received frame", frame);
    if (maester == NULL || frame == NULL) {
        return;
    }

    // Check if this frame is for us or needs forwarding
    if (my_strcasecmp(frame->destination, maester->realm_name) != 0) {
        // Frame is NOT for us - forward it to the next hop
        write_str(STDOUT_FILENO, "Forwarding frame from ");
        write_str(STDOUT_FILENO, frame->origin);
        write_str(STDOUT_FILENO, " to ");
        write_str(STDOUT_FILENO, frame->destination);
        write_str(STDOUT_FILENO, " via next hop...\n");

        // Resolve route to destination
        int used_default = 0;
        Route* route = maester_resolve_route(maester, frame->destination, &used_default);

        if (route == NULL) {
            // No route found - send ERROR_UNKNOWN back to origin
            write_str(STDERR_FILENO, "No route to ");
            write_str(STDERR_FILENO, frame->destination);
            write_str(STDERR_FILENO, ", sending ERROR_UNKNOWN to origin.\n");

            CitadelFrame error_frame;
            frame_init(&error_frame, FRAME_TYPE_ERROR_UNKNOWN, maester->realm_name, frame->origin);
            const char* error_msg = "No route to destination";
            int msg_len = my_strlen(error_msg);
            if (msg_len > FRAME_MAX_DATA) msg_len = FRAME_MAX_DATA;
            memcpy(error_frame.data, error_msg, msg_len);
            error_frame.data_length = msg_len;

            // Send error back through the connection we received from
            maester_send_frame(entry, &error_frame);
            return;
        }

        // Get or open connection to next hop
        ConnectionEntry* next_hop = maester_get_or_open_connection(maester, route->realm, route->ip, route->port);
        if (next_hop == NULL) {
            write_str(STDERR_FILENO, "Failed to connect to next hop. Dropping frame.\n");
            return;
        }

        // Forward the frame
        if (maester_send_frame(next_hop, frame) == 0) {
            write_str(STDOUT_FILENO, "Frame forwarded successfully.\n");
        } else {
            write_str(STDERR_FILENO, "Failed to forward frame.\n");
        }
        return;
    }

    // Frame IS for us - process it locally
    if (frame->type == FRAME_TYPE_PLEDGE_RESPONSE &&
        maester_mission_is_active(maester) &&
        maester->active_mission.type == FRAME_TYPE_PLEDGE) {
        char message[128];
        int data_len = (frame->data_length < FRAME_MAX_DATA) ? frame->data_length : FRAME_MAX_DATA;
        char response[64];
        int copy_len = (data_len < (int)sizeof(response) - 1) ? data_len : (int)sizeof(response) - 1;
        memcpy(response, frame->data, copy_len);
        response[copy_len] = '\0';
        int written = snprintf(message, sizeof(message), "Alliance response from %s: %s",
                               frame->origin, response);
        if (written < 0 || written >= (int)sizeof(message)) {
            message[sizeof(message) - 1] = '\0';
        }
        maester_mission_finish(maester, message);
    }
}

// ========== Signal handler for CTRL+C ==========
// Note: g_should_exit is declared in main.c as required by coding standards

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
    maester_mission_check_timeouts(maester);
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
            if (token_count >= 3) {
                if (maester_mission_is_active(maester)) {
                    maester_mission_print_busy(maester, "a trade mission");
                    return;
                }
                char desc[64];
                desc[0] = '\0';
                str_append(desc, "Trade with ");
                str_append(desc, tokens[2]);
                if (!maester_mission_begin(maester, FRAME_TYPE_ORDER_HEADER, tokens[2], desc, 0)) {
                    return;
                }
                cmd_start_trade(maester, tokens[2]);
                maester_mission_finish(maester, "Trade mission finished.");
            } else {
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

        // Add connection to pool
        ConnectionEntry* entry = maester_add_connection_entry(maester);
        if (entry == NULL) {
            write_str(STDERR_FILENO, "\nWarning: Connection pool full, rejecting incoming connection.\n");
            close(client_fd);
            continue;
        }

        // Set socket to non-blocking
        if (set_socket_nonblocking(client_fd) < 0) {
            write_str(STDERR_FILENO, "Warning: Failed to set non-blocking mode on accepted socket.\n");
        }

        // Initialize connection entry
        entry->sockfd = client_fd;
        entry->addr = addr;
        entry->last_used = time(NULL);

        // Extract peer IP and port from addr structure
        char peer_ip[IP_ADDR_MAX];
        inet_ntop(AF_INET, &addr.sin_addr, peer_ip, sizeof(peer_ip));
        int peer_port = ntohs(addr.sin_port);
        my_strcpy(entry->peer_ip, peer_ip);
        entry->peer_port = peer_port;
        entry->peer_realm[0] = '\0';  // Unknown realm until we receive a frame

        // Log the connection
        write_str(STDOUT_FILENO, "\nAccepted connection from ");
        write_str(STDOUT_FILENO, peer_ip);
        write_str(STDOUT_FILENO, ":");
        char port_buf[16];
        int_to_str(peer_port, port_buf);
        write_str(STDOUT_FILENO, port_buf);
        write_str(STDOUT_FILENO, "\n");
    }
}

static void maester_event_loop(Maester* maester) {
    if (maester == NULL) return;

    int need_prompt = 1;
    char input[256];

    while (!g_should_exit && !maester->shutting_down) {
        maester_mission_check_timeouts(maester);
        maester_compact_connections(maester);

        if (need_prompt) {
            write_str(STDOUT_FILENO, "$ ");
            need_prompt = 0;
        }

        int poll_capacity = 1; // stdin
        if (maester->listen_fd >= 0) {
            poll_capacity++;
        }
        poll_capacity += maester->num_connections;
        if (poll_capacity < 2) poll_capacity = 2;

        struct pollfd pollfds[poll_capacity];
        int poll_index = 0;

        pollfds[poll_index].fd = STDIN_FILENO;
        pollfds[poll_index].events = POLLIN;
        pollfds[poll_index].revents = 0;
        poll_index++;

        int listen_index = -1;
        if (maester->listen_fd >= 0) {
            listen_index = poll_index;
            pollfds[poll_index].fd = maester->listen_fd;
            pollfds[poll_index].events = POLLIN;
            pollfds[poll_index].revents = 0;
            poll_index++;
        }

        int conn_slots = (maester->num_connections > 0) ? maester->num_connections : 1;
        ConnectionEntry* conn_map[conn_slots];
        int conn_start = poll_index;
        int conn_count = 0;
        for (int i = 0; i < maester->num_connections; i++) {
            if (maester->connections[i].sockfd < 0) {
                continue;
            }
            pollfds[poll_index].fd = maester->connections[i].sockfd;
            pollfds[poll_index].events = POLLIN;
            if (maester_connection_has_pending_send(&maester->connections[i])) {
                pollfds[poll_index].events |= POLLOUT;
            }
            pollfds[poll_index].revents = 0;
            conn_map[conn_count++] = &maester->connections[i];
            poll_index++;
        }

        int nfds = poll_index;

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

        if (listen_index != -1 && listen_index < nfds) {
            if (pollfds[listen_index].revents & POLLIN) {
                maester_accept_placeholder(maester);
                need_prompt = 1;
            } else if (pollfds[listen_index].revents & (POLLHUP | POLLERR)) {
                write_str(STDERR_FILENO, "Listener socket reported an error.\n");
            }
        }

        for (int i = 0; i < conn_count; i++) {
            ConnectionEntry* entry = conn_map[i];
            short revents = pollfds[conn_start + i].revents;
            if (revents != 0 && entry != NULL) {
                maester_handle_connection_event(maester, entry, revents);
            }
        }

        maester_compact_connections(maester);
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
    maester_close_all_connections(maester);

    write_str(STDOUT_FILENO, "\nCleaning up resources...\n");
    free_maester(maester);
    write_str(STDOUT_FILENO, "Maester process terminated. Farewell.\n");
    return 0;
}
