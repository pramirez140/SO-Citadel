#ifndef TRADE_H
#define TRADE_H

#include "stock.h"
#include "helper.h"

struct Maester;

Product* trade_load_realm_stock(const char* realm, int* num_products);

void cmd_start_trade(struct Maester* maester, const char* realm);

#endif
