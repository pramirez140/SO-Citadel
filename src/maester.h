#ifndef MAESTER_H
#define MAESTER_H

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "stock.h"
#include "helper.h"

typedef struct {
    char realm[50];
    char ip[20];
    int  port;
} Route;

typedef struct Maester {
    char realm_name[50];
    char folder_path[200];
    int  num_envoys;
    char ip[20];
    int  port;
    int  socket_fd;
    Route*   routes;
    int      num_routes;
    Product* stock;
    int      num_products;
 } Maester;


Maester* create_maester(const char* realm_name, const char* folder_path, const char* ip, int port);
void     add_route(Maester* maester, const char* realm, const char* ip, int port);
void     add_product(Maester* maester, const char* name, float price, int quantity);
void     free_maester(Maester* maester);

// Load maester from configuration files
Maester* load_maester_config(const char* config_file, const char* stock_file);

// Command handlers for Phase 1
void cmd_list_realms(Maester* maester);
void cmd_list_products(Maester* maester, const char* realm);
void cmd_pledge(Maester* maester, const char* realm, const char* sigil);
void cmd_pledge_respond(Maester* maester, const char* realm, const char* response);
void cmd_pledge_status(Maester* maester);
void cmd_envoy_status(Maester* maester);
// Runs the Maester
int maester_run(const char* config_file, const char* stock_file);

#endif
