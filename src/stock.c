#include "stock.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

Product* load_stock(const char* filename, int* num_products) {
    char path[512];
    int has_slash = 0;
    for (int i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '/') { 
            has_slash = 1;
            break; 
        }
    }

    if (has_slash) {
        my_strcpy(path, filename);
    } else {
        my_strcpy(path, "data/");
        str_append(path, filename);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        write_str(STDERR_FILENO, "Error: Cannot open stock database file ");
        write_str(STDERR_FILENO, filename);
        write_str(STDERR_FILENO, "\n");
        *num_products = 0;
        return NULL;
    }

    // Get file size to calculate number of products
    long long file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (file_size < 0) {
        write_str(STDERR_FILENO, "Error: Cannot determine file size\n");
        close(fd);
        *num_products = 0;
        return NULL;
    }

    int count = file_size / sizeof(Product);
    *num_products = count;

    if (count == 0) {
        close(fd);
        return NULL;
    }

    // Allocate memory for products
    Product* stock = (Product*)malloc(count * sizeof(Product));
    if (stock == NULL) {
        write_str(STDERR_FILENO, "Error: Memory allocation failed for stock\n");
        close(fd);
        *num_products = 0;
        return NULL;
    }

    // Read all products from file
    long bytes_read = read(fd, stock, count * sizeof(Product));
    if (bytes_read != (long)(count * sizeof(Product))) {
        write_str(STDERR_FILENO, "Error: Failed to read stock database\n");
        free(stock);
        close(fd);
        *num_products = 0;
        return NULL;
    }

    close(fd);
    return stock;
}

void free_stock(Product* stock) {
    if (stock != NULL) {
        free(stock);
    }
}

void print_products(int numProducts, Product *products) {
    if (numProducts == 0 || products == NULL) {
        write_str(STDOUT_FILENO, "No products in stock.\n");
        return;
    }

    // Header
    write_str(STDOUT_FILENO, "--- Trade Ledger ---\n");
    write_str(STDOUT_FILENO, "Item                      | Value (Gold) | Weight (Stone)\n");
    write_str(STDOUT_FILENO, "--------------------------------------------------------\n");

    char buf[64];
    for (int i = 0; i < numProducts; i++) {
        // Name - pad to 25 characters
        write_str(STDOUT_FILENO, products[i].name);
        int len = my_strlen(products[i].name);
        for (int j = len; j < 25; j++)
            write_str(STDOUT_FILENO, " ");

        write_str(STDOUT_FILENO, " | ");

        // Value (amount) - pad to 12 characters (right-aligned)
        int_to_str(products[i].amount, buf);
        int val_len = my_strlen(buf);
        for (int j = val_len; j < 12; j++)
            write_str(STDOUT_FILENO, " ");
        write_str(STDOUT_FILENO, buf);

        write_str(STDOUT_FILENO, " | ");

        // Weight (float with one decimal)
        int int_part = (int)products[i].weight;
        int decimal = (int)((products[i].weight - int_part) * 10);

        int_to_str(int_part, buf);
        write_str(STDOUT_FILENO, buf);
        write_str(STDOUT_FILENO, ".");
        char dec_buf[4];
        int_to_str(decimal, dec_buf);
        write_str(STDOUT_FILENO, dec_buf);

        write_str(STDOUT_FILENO, "\n");
    }

    // Footer separator
    write_str(STDOUT_FILENO, "--------------------------------------------------------\n");

    // Total count
    write_str(STDOUT_FILENO, "Total Entries: ");
    char total_buf[10];
    int_to_str(numProducts, total_buf);
    write_str(STDOUT_FILENO, total_buf);
    write_str(STDOUT_FILENO, "\n");
}