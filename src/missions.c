#include "missions.h"
#include "network.h"

static void maester_mission_reset(Maester* maester);

void maester_mission_init(Maester* maester) {
    maester_mission_reset(maester);
}

int maester_mission_is_active(const Maester* maester) {
    return (maester != NULL && maester->active_mission.in_use);
}

static void maester_mission_reset(Maester* maester) {
    if (maester == NULL) {
        return;
    }
    maester->active_mission.in_use = 0;
    maester->active_mission.type = FRAME_TYPE_PLEDGE;
    maester->active_mission.target_realm[0] = '\0';
    maester->active_mission.description[0] = '\0';
    maester->active_mission.started_at = 0;
    maester->active_mission.deadline = 0;
}

void maester_mission_print_busy(const Maester* maester, const char* new_action) {
    if (maester == NULL || !maester_mission_is_active(maester)) return;
    write_str(STDOUT_FILENO, "Cannot start ");
    if (new_action != NULL) {
        write_str(STDOUT_FILENO, new_action);
    } else {
        write_str(STDOUT_FILENO, "a new mission");
    }
    write_str(STDOUT_FILENO, ". Mission in progress: ");
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
}

int maester_mission_begin(Maester* maester, FrameType type, const char* target, const char* description, int timeout_seconds) {
    if (maester == NULL) return 0;
    if (maester_mission_is_active(maester)) {
        maester_mission_print_busy(maester, description);
        return 0;
    }

    maester->active_mission.in_use = 1;
    maester->active_mission.type = type;
    maester->active_mission.started_at = time(NULL);
    if (target != NULL) {
        my_strcpy(maester->active_mission.target_realm, target);
    } else {
        maester->active_mission.target_realm[0] = '\0';
    }
    if (description != NULL) {
        my_strcpy(maester->active_mission.description, description);
    } else {
        maester->active_mission.description[0] = '\0';
    }
    if (timeout_seconds > 0) {
        maester->active_mission.deadline = maester->active_mission.started_at + timeout_seconds;
    } else {
        maester->active_mission.deadline = 0;
    }

    if (description != NULL) {
        write_str(STDOUT_FILENO, "Mission started: ");
        write_str(STDOUT_FILENO, description);
    } else {
        write_str(STDOUT_FILENO, "Mission started.");
    }
    if (maester->active_mission.target_realm[0] != '\0') {
        write_str(STDOUT_FILENO, " Target: ");
        write_str(STDOUT_FILENO, maester->active_mission.target_realm);
    }
    if (timeout_seconds > 0) {
        write_str(STDOUT_FILENO, " (timeout ");
        char buf[16];
        int_to_str(timeout_seconds, buf);
        write_str(STDOUT_FILENO, buf);
        write_str(STDOUT_FILENO, "s)");
    }
    write_str(STDOUT_FILENO, ".\n");
    return 1;
}

void maester_mission_finish(Maester* maester, const char* log_message) {
    if (!maester_mission_is_active(maester)) {
        return;
    }
    if (log_message != NULL) {
        write_str(STDOUT_FILENO, log_message);
        write_str(STDOUT_FILENO, "\n");
    }
    maester_mission_reset(maester);
}

void maester_mission_check_timeouts(Maester* maester) {
    if (!maester_mission_is_active(maester)) return;
    if (maester->active_mission.deadline == 0) return;
    time_t now = time(NULL);
    if (now >= maester->active_mission.deadline) {
        write_str(STDOUT_FILENO, "Mission timed out: ");
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
        maester_mission_reset(maester);
    }
}
