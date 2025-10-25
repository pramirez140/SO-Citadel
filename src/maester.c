#include "maester.h"
#include "trade.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#define MAX_LINE_LENGTH 256

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
    char realm_name[50];
    char folder_path[200];
    char ip[20];
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

    // Create folder if it doesn't exist (mkdir with 0755 permissions)
    mkdir(folder_path, 0755);  // Ignore error if folder already exists

    // Read "--- ROUTES ---" line
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        free_maester(maester);
        return NULL;
    }

    // Read routes
    if (read_line_fd(fd, line, MAX_LINE_LENGTH) <= 0) {
        // Parse: realm ip port
        char route_realm[50];
        char route_ip[20];
        int route_port = 0;

        // Simple parsing: find spaces
        int i = 0, j = 0;

        // Get realm name
        while (line[i] != ' ' && line[i] != '\0') {
            route_realm[j++] = line[i++];
        }
        route_realm[j] = '\0';

        // Skip spaces
        while (line[i] == ' ') i++;

        // Get IP
        j = 0;
        while (line[i] != ' ' && line[i] != '\0') {
            route_ip[j++] = line[i++];
        }
        route_ip[j] = '\0';

        // Skip spaces
        while (line[i] == ' ') i++;

        // Get port
        char port_str[20];
        j = 0;
        while (line[i] != '\0') {
            port_str[j++] = line[i++];
        }
        port_str[j] = '\0';
        route_port = str_to_int(port_str);

        // Add route
        add_route(maester, route_realm, route_ip, route_port);
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

    if (maester->routes != NULL) {
        free(maester->routes);
    }

    if (maester->stock != NULL) {
        free(maester->stock);
    }

    free(maester);
}

// ============= COMMAND HANDLERS PHASE 1 =============

void cmd_list_realms(Maester* maester) {
    if (maester == NULL) return;

    write_str(STDOUT_FILENO, "Known Realms:\n");
    for (int i = 0; i < maester->num_routes; i++) {
        // Skip DEFAULT - it's not a realm, it's the default route
        if (my_strcasecmp(maester->routes[i].realm, "DEFAULT") == 0) {
            continue;
        }

        write_str(STDOUT_FILENO, "  - ");
        write_str(STDOUT_FILENO, maester->routes[i].realm);

        // Indicate if route is known or unknown
        if (my_strcmp(maester->routes[i].ip, "*.*.*.*") == 0) {
            write_str(STDOUT_FILENO, " (route unknown)");
        }
        write_str(STDOUT_FILENO, "\n");
    }
}

void cmd_list_products(Maester* maester, const char* realm) {
    if (maester == NULL) return;

    // Phase 1: Only implement listing our own products (without realm)
    if (realm == NULL || my_strlen(realm) == 0) {
        // Fully implemented for Phase 1 - use stock.c function
        print_products(maester->num_products, maester->stock);
    } else {
        // Phase 1: Load products from realm's database file
        write_str(STDOUT_FILENO, "Listing products from ");
        write_str(STDOUT_FILENO, realm);
        write_str(STDOUT_FILENO, ":\n");

        int num_products = 0;
        Product* realm_stock = trade_load_realm_stock(realm, &num_products);

        if (realm_stock != NULL && num_products > 0) {
            // Display products in simple numbered list
            for (int i = 0; i < num_products; i++) {
                char num_buf[20];
                int_to_str(i + 1, num_buf);
                write_str(STDOUT_FILENO, num_buf);
                write_str(STDOUT_FILENO, ". ");
                write_str(STDOUT_FILENO, realm_stock[i].name);
                write_str(STDOUT_FILENO, " (");
                int_to_str(realm_stock[i].amount, num_buf);
                write_str(STDOUT_FILENO, num_buf);
                write_str(STDOUT_FILENO, " units)\n");
            }
            free_stock(realm_stock);
        } else {
            write_str(STDOUT_FILENO, "No products available from this realm.\n");
        }
    }
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
        return;
    }

    write_str(STDOUT_FILENO, "Unknown command\n");
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

    char input[256];

    while (!g_should_exit) {
        write_str(STDOUT_FILENO, "$ ");

        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                if (g_should_exit) break;
                continue;
            }
            break; // other read error
        }
        if (bytes_read == 0 || g_should_exit) break;

        input[bytes_read] = '\0';
        for (int i = 0; i < bytes_read; i++) {
            if (input[i] == '\n' || input[i] == '\r') { input[i] = '\0'; break; }
        }
        if (my_strlen(input) > 0) process_command(maester, input);
    }

    write_str(STDOUT_FILENO, "\nCleaning up resources...\n");
    free_maester(maester);
    write_str(STDOUT_FILENO, "Maester process terminated. Farewell.\n");
    return 0;
}