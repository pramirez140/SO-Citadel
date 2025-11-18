#ifndef NETWORK_H
#define NETWORK_H

#include "maester.h"

// Frame helpers
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

// Routing helpers
Route* maester_resolve_route(Maester* maester, const char* destination, int* used_default);
void   maester_log_route_resolution(const char* destination, const Route* route, int used_default);

// Connections
ConnectionEntry* maester_get_or_open_connection(Maester* maester, const char* realm, const char* ip, int port);
void             maester_compact_connections(Maester* maester);
void             maester_close_all_connections(Maester* maester);
void             maester_close_connection_entry(ConnectionEntry* entry);
int              maester_send_frame(ConnectionEntry* entry, const CitadelFrame* frame);
int              maester_connection_has_pending_send(const ConnectionEntry* entry);
void             maester_flush_send_buffer(ConnectionEntry* entry);

#endif
