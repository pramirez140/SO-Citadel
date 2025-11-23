#ifndef STOCK_H
#define STOCK_H

#include <fcntl.h>
#include <unistd.h>
#include "helper.h"

typedef struct {
    char  name[100];
    int   amount;
    float weight;
} Product;

// Stock database functions
Product* load_stock(const char* filename, int* num_products);
int      save_stock(const char* filename, Product* stock, int num_products);  // Saves stock data to binary file
void     free_stock(Product* stock);
void     print_products(int num_products, Product* products);

#endif
