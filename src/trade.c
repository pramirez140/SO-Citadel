#include "trade.h"
#include "helper.h"
#include "stock.h"
#include "maester.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// ------- realm stock loader -------
Product* trade_load_realm_stock(const char* realm, int* num_products) {
    char filename[300];
    my_strcpy(filename, "data/realm_stock/");

    // filename uses lowercase realm
    char realm_lower[100];
    str_tolower(realm_lower, realm);
    str_append(filename, realm_lower);
    str_append(filename, "_stock.db");

    return load_stock(filename, num_products);
}

typedef struct {
    char product_name[100];
    int  quantity;
} ShoppingItem;

void cmd_start_trade(struct Maester* maester, const char* realm) {
    if (!maester || !realm) return;

    write_str(STDOUT_FILENO, "Entering trade mode with ");
    write_str(STDOUT_FILENO, realm);
    write_str(STDOUT_FILENO, ".\n");

    int num_products = 0;
    Product* realm_stock = trade_load_realm_stock(realm, &num_products);

    if (!realm_stock || num_products == 0) {
        write_str(STDOUT_FILENO, "No products available from this realm.\n");
        if (realm_stock) free_stock(realm_stock);
        return;
    }

    // Show available products
    write_str(STDOUT_FILENO, "Available products: ");
    for (int i = 0; i < num_products; i++) {
        write_str(STDOUT_FILENO, realm_stock[i].name);
        if (i < num_products - 1) write_str(STDOUT_FILENO, ", ");
    }
    write_str(STDOUT_FILENO, ".\n");

    ShoppingItem shopping_list[50];
    int item_count = 0;

    char input[256];
    while (1) {
        write_str(STDOUT_FILENO, "(trade)> ");

        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                write_str(STDOUT_FILENO, "\nTrade cancelled.\n");
                free_stock(realm_stock);
                return;
            }
            break;
        }
        if (bytes_read == 0) break;

        input[bytes_read] = '\0';
        for (int i = 0; i < bytes_read; i++) {
            if (input[i] == '\n' || input[i] == '\r') { input[i] = '\0'; break; }
        }
        if (my_strlen(input) == 0) continue;

        // simple tokenizer
        char* tokens[10];
        int token_count = 0, i = 0, in_token = 0;
        while (input[i] != '\0' && token_count < 10) {
            if (input[i] == ' ' || input[i] == '\t') {
                if (in_token) { input[i] = '\0'; in_token = 0; }
            } else {
                if (!in_token) { tokens[token_count++] = &input[i]; in_token = 1; }
            }
            i++;
        }

        // send -> write file and exit
        if (my_strcasecmp(tokens[0], "send") == 0) {
            char filename[256];
            my_strcpy(filename, maester->folder_path);
            str_append(filename, "/trade_");
            str_append(filename, realm);
            str_append(filename, ".txt");

            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                write_str(fd, "TRADE ORDER\nFrom: ");
                write_str(fd, maester->realm_name);
                write_str(fd, "\nTo: ");
                write_str(fd, realm);
                write_str(fd, "\n\nItems:\n");
                for (int j = 0; j < item_count; j++) {
                    write_str(fd, shopping_list[j].product_name);
                    write_str(fd, " - ");
                    char qty_buf[20]; int_to_str(shopping_list[j].quantity, qty_buf);
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

        // cancel -> exit
        if (my_strcasecmp(tokens[0], "cancel") == 0) {
            write_str(STDOUT_FILENO, "Trade cancelled.\n");
            free_stock(realm_stock);
            break;
        }

        // add <product> <qty>
        if (my_strcasecmp(tokens[0], "add") == 0) {
            if (token_count >= 3) {
                char product_name[100]; product_name[0] = '\0';
                int qty_index = token_count - 1;
                int quantity = str_to_int(tokens[qty_index]);

                for (int j = 1; j < qty_index; j++) {
                    if (j > 1) str_append(product_name, " ");
                    str_append(product_name, tokens[j]);
                }

                int found = 0, available = 0;
                for (int k = 0; k < num_products; k++) {
                    if (my_strcasecmp(product_name, realm_stock[k].name) == 0) {
                        found = 1; available = realm_stock[k].amount; break;
                    }
                }

                if (!found) {
                    write_str(STDOUT_FILENO, "No product matches '");
                    write_str(STDOUT_FILENO, product_name);
                    write_str(STDOUT_FILENO, "'. Please check available products.\n");
                } else if (quantity <= 0) {
                    write_str(STDOUT_FILENO, "Quantity must be > 0\n");
                } else {
                    if (quantity > available) {
                        write_str(STDOUT_FILENO, "Warning: Only ");
                        char b[20]; int_to_str(available, b);
                        write_str(STDOUT_FILENO, b);
                        write_str(STDOUT_FILENO, " units available. Requested: ");
                        int_to_str(quantity, b); write_str(STDOUT_FILENO, b);
                        write_str(STDOUT_FILENO, "\n");
                    }
                    if (item_count < 50) {
                        my_strcpy(shopping_list[item_count].product_name, product_name);
                        shopping_list[item_count].quantity = quantity;
                        item_count++;
                    } else {
                        write_str(STDOUT_FILENO, "Shopping cart is full!\n");
                    }
                }
            } else {
                write_str(STDOUT_FILENO, "Usage: add <product> <quantity>\n");
            }
            continue;
        }

        // remove <product> <qty>
        if (my_strcasecmp(tokens[0], "remove") == 0) {
            if (token_count >= 3) {
                char product_name[100]; product_name[0] = '\0';
                int qty_index = token_count - 1;
                int quantity = str_to_int(tokens[qty_index]);
                for (int j = 1; j < qty_index; j++) {
                    if (j > 1) str_append(product_name, " ");
                    str_append(product_name, tokens[j]);
                }

                int removed = 0;
                for (int j = 0; j < item_count; j++) {
                    if (my_strcasecmp(product_name, shopping_list[j].product_name) == 0) {
                        if (quantity >= shopping_list[j].quantity) {
                            // remove entry by shifting left
                            for (int k = j; k < item_count - 1; k++)
                                shopping_list[k] = shopping_list[k + 1];
                            item_count--;
                        } else {
                            shopping_list[j].quantity -= quantity;
                        }
                        removed = 1; break;
                    }
                }
                if (!removed) {
                    write_str(STDOUT_FILENO, "Item not in cart.\n");
                }
            } else {
                write_str(STDOUT_FILENO, "Usage: remove <product> <quantity>\n");
            }
            continue;
        }

        write_str(STDOUT_FILENO, "Unknown trade command. Try: add <product> <quantity>, remove <product> <quantity>, send, cancel\n");
    }
}
