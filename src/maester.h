#ifndef MAESTER_H
#define MAESTER_H

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "stock.h"
#include "helper.h"

#define REALM_NAME_MAX   64
#define IP_ADDR_MAX      46
#define PATH_MAX_LEN     256
#define ROUTE_DEFAULT    "DEFAULT"

typedef enum {
    ALLIANCE_UNKNOWN = 0,
    ALLIANCE_PENDING,
    ALLIANCE_ACTIVE,
    ALLIANCE_INACTIVE
} AllianceState;

typedef enum {
    ENVOY_TYPE_NONE = 0,
    ENVOY_TYPE_PLEDGE,
    ENVOY_TYPE_PRODUCT_LIST,
    ENVOY_TYPE_TRADE
} EnvoyMissionType;

typedef enum {
    ENVOY_STATUS_IDLE = 0,
    ENVOY_STATUS_RUNNING,
    ENVOY_STATUS_SUCCESS,
    ENVOY_STATUS_TIMEOUT,
    ENVOY_STATUS_FAILED
} EnvoyMissionStatus;

typedef enum {
    FRAME_TYPE_PLEDGE            = 0x01,
    FRAME_TYPE_PLEDGE_RESPONSE   = 0x03,
    FRAME_TYPE_LIST_REQUEST      = 0x11,
    FRAME_TYPE_LIST_RESPONSE     = 0x12,
    FRAME_TYPE_ORDER_HEADER      = 0x14,
    FRAME_TYPE_ORDER_DATA        = 0x15,
    FRAME_TYPE_ORDER_RESPONSE    = 0x16,
    FRAME_TYPE_DISCONNECT        = 0x27,
    FRAME_TYPE_ERROR_UNKNOWN     = 0x21,
    FRAME_TYPE_ERROR_UNAUTHORIZED= 0x25,
    FRAME_TYPE_ACK_FILE          = 0x31,
    FRAME_TYPE_ACK_MD5           = 0x32,
    FRAME_TYPE_NACK              = 0x69
} FrameType;

typedef struct {
    char realm[REALM_NAME_MAX];
    char ip[IP_ADDR_MAX];
    int  port;
} Route;

typedef struct {
    FrameType  type;
    char       origin[REALM_NAME_MAX];
    char       destination[REALM_NAME_MAX];
    uint32_t   data_length;
    char      *data;
    uint32_t   checksum;
} CitadelFrame;

typedef struct {
    char          realm[REALM_NAME_MAX];
    char          ip[IP_ADDR_MAX];
    int           port;
    AllianceState state;
    time_t        last_status;
} AllianceEntry;

typedef struct {
    EnvoyMissionType   type;
    EnvoyMissionStatus status;
    char               target_realm[REALM_NAME_MAX];
    char               payload_path[PATH_MAX_LEN];
    time_t             started_at;
} EnvoyMission;

typedef struct {
    int                sockfd;
    char               peer_realm[REALM_NAME_MAX];
    struct sockaddr_in addr;
    time_t             last_used;
} ConnectionEntry;

typedef struct {
    CitadelFrame   *buffer;
    size_t          capacity;
    size_t          count;
    size_t          head;
    size_t          tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} FrameQueue;

typedef struct {
    int       in_use;
    FrameType type;
    char      target_realm[REALM_NAME_MAX];
    time_t    deadline;
} MissionState;

typedef struct Maester {
    char realm_name[REALM_NAME_MAX];
    char folder_path[PATH_MAX_LEN];
    int  num_envoys;
    char ip[IP_ADDR_MAX];
    int  port;
    int  socket_fd;
    Route*   routes;
    int      num_routes;
    Product* stock;
    int      num_products;
    AllianceEntry*   alliances;
    int              num_alliances;
    EnvoyMission*    envoy_missions;
    ConnectionEntry* connections;
    int              num_connections;
    FrameQueue       outbound_queue;
    int              listen_fd;
    pthread_t        listener_thread;
    pthread_mutex_t  routes_lock;
    pthread_mutex_t  alliances_lock;
    pthread_mutex_t  envoys_lock;
    pthread_mutex_t  connections_lock;
    int              shutting_down;
    MissionState     active_mission;
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
