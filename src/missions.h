#ifndef MISSIONS_H
#define MISSIONS_H

#include "maester.h"

void maester_mission_init(Maester* maester);
int  maester_mission_is_active(const Maester* maester);
void maester_mission_print_busy(const Maester* maester, const char* new_action);
int  maester_mission_begin(Maester* maester, FrameType type, const char* target, const char* description, int timeout_seconds);
void maester_mission_finish(Maester* maester, const char* log_message);
void maester_mission_check_timeouts(Maester* maester);

#endif
