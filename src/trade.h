#ifndef TRADE_H
#define TRADE_H

#include "maester.h"
#include "helper.h"
#include "stock.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void cmd_start_trade(struct Maester* maester, const char* realm);

#endif
