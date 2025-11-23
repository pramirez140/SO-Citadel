#ifndef MAESTER_H
#define MAESTER_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>   // Needed for mkdir() only (stat() function is forbidden and not used)
#include <sys/types.h>
#include <time.h>

#include "stock.h"
#include "helper.h"

// Global variable declared in main.c (signal handling)
extern volatile sig_atomic_t g_should_exit;

#define REALM_NAME_MAX        64
#define IP_ADDR_MAX           46
#define PATH_MAX_LEN          256
#define ROUTE_DEFAULT         "DEFAULT"

#define FRAME_ORIGIN_LEN      20
#define FRAME_DEST_LEN        20
#define FRAME_MAX_DATA        275
#define FRAME_HEADER_LEN      (1 + FRAME_ORIGIN_LEN + FRAME_DEST_LEN + 2)
#define FRAME_CHECKSUM_LEN    2
#define FRAME_MAX_SIZE        320  // Fixed size per protocol specification
#define FRAME_BUFFER_CAPACITY (FRAME_MAX_SIZE * 4)

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
    char       origin[FRAME_ORIGIN_LEN + 1];
    char       destination[FRAME_DEST_LEN + 1];
    uint16_t   data_length;
    uint8_t    data[FRAME_MAX_DATA];
    uint16_t   checksum;
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
    CitadelFrame   *buffer;
    size_t          capacity;
    size_t          count;
    size_t          head;
    size_t          tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} FrameQueue;

typedef enum {
    FRAME_PARSE_OK = 0,
    FRAME_PARSE_NEED_MORE = 1,
    FRAME_PARSE_INVALID = -1,
    FRAME_PARSE_BAD_CHECKSUM = -2
} FrameParseResult;

typedef struct {
    int       in_use;
    FrameType type;
    char      target_realm[REALM_NAME_MAX];
    char      description[64];
    time_t    started_at;
    time_t    deadline;
} MissionState;

typedef struct {
    uint8_t data[FRAME_BUFFER_CAPACITY];
    size_t  length;
} FrameBuffer;

typedef struct {
    int                sockfd;
    char               peer_realm[REALM_NAME_MAX];
    char               peer_ip[IP_ADDR_MAX];
    int                peer_port;
    struct sockaddr_in addr;
    time_t             last_used;
    FrameBuffer        recv_buffer;
    FrameBuffer        send_buffer;
} ConnectionEntry;

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
    char     stock_file_path[PATH_MAX_LEN];  // Path to stock database file
    AllianceEntry*   alliances;
    int              num_alliances;
    EnvoyMission*    envoy_missions;
    ConnectionEntry* connections;
    int              num_connections;
    int              connections_capacity;
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

// Frame helpers (Phase 2 networking)
void             frame_init(CitadelFrame* frame, FrameType type, const char* origin, const char* destination);
int              frame_serialize(const CitadelFrame* frame, uint8_t* buffer, size_t buffer_len, size_t* out_len);
FrameParseResult frame_deserialize(const uint8_t* buffer, size_t length, CitadelFrame* frame, size_t* consumed_bytes);
uint16_t         frame_compute_checksum_bytes(const uint8_t* buffer, size_t length);
void             frame_log_summary(const char* prefix, const CitadelFrame* frame);
const char*      frame_type_to_string(FrameType type);

void frame_buffer_init(FrameBuffer* fb);
void frame_buffer_reset(FrameBuffer* fb);
int  frame_buffer_append(FrameBuffer* fb, const uint8_t* data, size_t length);
FrameParseResult frame_buffer_extract(FrameBuffer* fb, CitadelFrame* frame, size_t* consumed_bytes);

#endif
