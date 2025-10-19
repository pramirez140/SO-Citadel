#include "maester.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_LINE_LENGTH 256

// Helper function to read a line from file descriptor
static int read_line(int fd, char *buffer, int max_len) {
    int i = 0;
    char c;

    while (i < max_len - 1) {
        ssize_t bytes = read(fd, &c, 1);
        if (bytes <= 0) {
            break;
        }
        if (c == '\n') {
            break;
        }
        if (c != '\r') {  // Skip carriage returns
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
    return i;
}

// Helper function to remove '&' character from realm names (as per instructions)
static void clean_realm_name(char *name) {
    int i = 0, j = 0;
    while (name[i] != '\0') {
        if (name[i] != '&') {
            name[j++] = name[i];
        }
        i++;
    }
    name[j] = '\0';
}

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
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(realm_name, line);

    // Read folder path
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(folder_path, line);

    // Read number of envoys
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    num_envoys = str_to_int(line);

    // Read IP
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        return NULL;
    }
    my_strcpy(ip, line);

    // Read port
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
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
    if (read_line(fd, line, MAX_LINE_LENGTH) <= 0) {
        close(fd);
        free_maester(maester);
        return NULL;
    }

    // Read routes
    while (read_line(fd, line, MAX_LINE_LENGTH) > 0) {
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

// ============= COMMAND HANDLERS FOR PHASE 1 =============

// Helper function to load a realm's stock from data/realm_stock/{realm}_stock.db
static Product* load_realm_stock(const char* realm, int* num_products) {
    char filename[300];
    my_strcpy(filename, "data/realm_stock/");

    // Convert realm name to lowercase for filename
    char realm_lower[100];
    str_tolower(realm_lower, realm);
    str_append(filename, realm_lower);
    str_append(filename, "_stock.db");

    // Load the stock using existing function
    return load_stock(filename, num_products);
}

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
        Product* realm_stock = load_realm_stock(realm, &num_products);

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

// Helper structure for shopping list
typedef struct {
    char product_name[100];
    int quantity;
} ShoppingItem;

void cmd_start_trade(Maester* maester, const char* realm) {
    if (maester == NULL || realm == NULL) return;

    write_str(STDOUT_FILENO, "Entering trade mode with ");
    write_str(STDOUT_FILENO, realm);
    write_str(STDOUT_FILENO, ".\n");

    // Load realm's products from database
    int num_products = 0;
    Product* realm_stock = load_realm_stock(realm, &num_products);

    if (realm_stock == NULL || num_products == 0) {
        write_str(STDOUT_FILENO, "No products available from this realm.\n");
        if (realm_stock) free_stock(realm_stock);
        return;
    }

    // Display available products
    write_str(STDOUT_FILENO, "Available products: ");
    for (int i = 0; i < num_products; i++) {
        write_str(STDOUT_FILENO, realm_stock[i].name);
        if (i < num_products - 1) {
            write_str(STDOUT_FILENO, ", ");
        }
    }
    write_str(STDOUT_FILENO, ".\n");

    // Shopping list storage
    ShoppingItem shopping_list[50];
    int item_count = 0;

    // Interactive trade loop
    char input[256];
    while (1) {
        write_str(STDOUT_FILENO, "(trade)> ");

        // Read input
        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);

        // Check if read was interrupted by signal (CTRL+C)
        if (bytes_read < 0) {
            if (errno == EINTR) {
                // Signal interrupted read - exit trade mode
                write_str(STDOUT_FILENO, "\nTrade cancelled.\n");
                free_stock(realm_stock);
                return;
            }
            break;
        }

        if (bytes_read == 0) break;

        input[bytes_read] = '\0';

        // Remove newline
        for (int i = 0; i < bytes_read; i++) {
            if (input[i] == '\n' || input[i] == '\r') {
                input[i] = '\0';
                break;
            }
        }

        // Parse command
        char *tokens[10];
        int token_count = 0;
        int i = 0, in_token = 0;

        while (input[i] != '\0' && token_count < 10) {
            if (input[i] == ' ' || input[i] == '\t') {
                if (in_token) {
                    input[i] = '\0';
                    in_token = 0;
                }
            } else {
                if (!in_token) {
                    tokens[token_count++] = &input[i];
                    in_token = 1;
                }
            }
            i++;
        }

        if (token_count == 0) continue;

        // Handle "send" command
        if (my_strcasecmp(tokens[0], "send") == 0) {
            // Save shopping list to file
            char filename[256];
            my_strcpy(filename, maester->folder_path);
            str_append(filename, "/trade_");
            str_append(filename, realm);
            str_append(filename, ".txt");

            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                write_str(fd, "TRADE ORDER\n");
                write_str(fd, "From: ");
                write_str(fd, maester->realm_name);
                write_str(fd, "\nTo: ");
                write_str(fd, realm);
                write_str(fd, "\n\nItems:\n");

                for (int j = 0; j < item_count; j++) {
                    write_str(fd, shopping_list[j].product_name);
                    write_str(fd, " - ");
                    char qty_buf[20];
                    int_to_str(shopping_list[j].quantity, qty_buf);
                    write_str(fd, qty_buf);
                    write_str(fd, " units\n");
                }
                close(fd);

                write_str(STDOUT_FILENO, "Trade list sent to ");
                write_str(STDOUT_FILENO, realm);
                write_str(STDOUT_FILENO, ".\n");
            } else {
                write_str(STDOUT_FILENO, "Error: Could not save trade list.\n");
            }
            free_stock(realm_stock);
            break;
        }

        // Handle "add" command
        if (my_strcasecmp(tokens[0], "add") == 0) {
            if (token_count >= 3) {
                // Get product name (may be multiple words, so join tokens 1 to n-1)
                char product_name[100];
                product_name[0] = '\0';

                // Find where the quantity is (last token should be a number)
                int qty_index = token_count - 1;
                int quantity = str_to_int(tokens[qty_index]);

                // Join all tokens from 1 to qty_index-1 as product name
                for (int j = 1; j < qty_index; j++) {
                    if (j > 1) str_append(product_name, " ");
                    str_append(product_name, tokens[j]);
                }

                // Validate: Check if product exists in realm's stock
                int product_found = 0;
                int available_qty = 0;
                for (int k = 0; k < num_products; k++) {
                    if (my_strcasecmp(product_name, realm_stock[k].name) == 0) {
                        product_found = 1;
                        available_qty = realm_stock[k].amount;
                        break;
                    }
                }

                if (!product_found) {
                    write_str(STDOUT_FILENO, "No product matches '");
                    write_str(STDOUT_FILENO, product_name);
                    write_str(STDOUT_FILENO, "'. Please check available products.\n");
                } else if (quantity > available_qty) {
                    write_str(STDOUT_FILENO, "Warning: Only ");
                    char qty_buf[20];
                    int_to_str(available_qty, qty_buf);
                    write_str(STDOUT_FILENO, qty_buf);
                    write_str(STDOUT_FILENO, " units available. Requested: ");
                    int_to_str(quantity, qty_buf);
                    write_str(STDOUT_FILENO, qty_buf);
                    write_str(STDOUT_FILENO, "\n");
                    // Still allow adding (Phase 1 doesn't enforce limits)
                    if (item_count < 50) {
                        my_strcpy(shopping_list[item_count].product_name, product_name);
                        shopping_list[item_count].quantity = quantity;
                        item_count++;
                    }
                } else {
                    // Add to shopping list
                    if (item_count < 50) {
                        my_strcpy(shopping_list[item_count].product_name, product_name);
                        shopping_list[item_count].quantity = quantity;
                        item_count++;
                    } else {
                        write_str(STDOUT_FILENO, "Shopping cart is full!\n");
                    }
                }
            } else if (token_count == 2) {
                // They typed "add <something>" but no quantity
                // Check if the product exists to give a helpful message
                char product_attempt[100];
                my_strcpy(product_attempt, tokens[1]);

                int product_exists = 0;
                for (int k = 0; k < num_products; k++) {
                    if (my_strcasecmp(product_attempt, realm_stock[k].name) == 0) {
                        product_exists = 1;
                        break;
                    }
                }

                if (product_exists) {
                    write_str(STDOUT_FILENO, "Missing quantity. Usage: add <product> <quantity>\n");
                } else {
                    write_str(STDOUT_FILENO, "No product matches '");
                    write_str(STDOUT_FILENO, product_attempt);
                    write_str(STDOUT_FILENO, "'. Please check available products.\n");
                }
            } else {
                write_str(STDOUT_FILENO, "Usage: add <product> <quantity>\n");
            }
        } else if (my_strcasecmp(tokens[0], "send") != 0) {
            write_str(STDOUT_FILENO, "Unknown trade command. Try: add <product> <quantity> or send\n");
        }
    }
}

void cmd_pledge(Maester* maester, const char* realm, const char* sigil) {
    if (maester == NULL || realm == NULL) return;

    (void)sigil; // Unused in Phase 1
    (void)realm; // Unused in Phase 1

    // Phase 1: Just acknowledge command
    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_pledge_respond(Maester* maester, const char* realm, const char* response) {
    if (maester == NULL || realm == NULL || response == NULL) return;

    (void)realm;    // Unused in Phase 1
    (void)response; // Unused in Phase 1

    // Phase 1: Just acknowledge command
    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_pledge_status(Maester* maester) {
    if (maester == NULL) return;

    // Phase 1: Just acknowledge command
    write_str(STDOUT_FILENO, "Command OK\n");
}

void cmd_envoy_status(Maester* maester) {
    if (maester == NULL) return;

    // Phase 1: Just acknowledge command
    write_str(STDOUT_FILENO, "Command OK\n");
}
