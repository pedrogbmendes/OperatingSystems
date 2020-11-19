#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

//#define ERROR_ID 0
#define PEER_ID_BASE 0x100000

/*
  Message format to communicate with the gateway.
  Transmited through  Sockets (UDP).
*/
// Gateway message types:
enum gw_msg_type { NULL_MSG_GW, INF_CREAT_PtG, READY_PtG,  REP_PING_PtG,
                   ACK_CREAT_GtP, NEW_PEER_GtP, PING_GtP, INF_DOWN_GtP,
                   NO_DOWN_GtP, REQ_DOWN_GtP, REQ_PEER_CtG, GIVE_PEER_GtC,
                   NO_PEER_GtC };
/*
    type             purpose               direction       address   port   id
    NULL_MSG_GW      null message          -- -> --        --        --     --
    INF_CREAT_PtG    inform creation       peer -> gw      --        x      --
    READY_PtG        ready to serve        peer -> gw      --        --     --
    REP_PING_PtG     reply ping            peer -> gw      --        --     --
    ACK_CREAT_GtP    acknowledge creation  gw -> peer      --        --     x
    NEW_PEER_GtP     inform new peer       gw -> peer      x         x      --
    PING_GtP         ping peer             gw -> peer      --        --     --
    INF_DOWN_GtP     inform download       gw -> peer      --        --     --
    NO_DOWN_GtP      inform no download    gw -> peer      --        --     --
    REQ_DOWN_GtP     request download      gw -> peer      --        --     --
    REQ_PEER_CtG     request peer          client -> gw    --        --     --
    GIVE_PEER_GtC    give peer             gw -> client    x         x      --
    NO_PEER_GtC      no peer available     gw -> client    --        --     --
*/

typedef struct message_gw {
  enum gw_msg_type type_m;    // Codifies the message purpose.
  char adress[20];            // Local adress (stream) of a peer.
  int port;                   // Local port of a peer *
  uint32_t id;                // Id of a photo.
} message_gw;


/*
  Message format to communicate between clients and peers (and between peers??).
  Transmited through Stream Sockets (TCP).
*/
// Source <-> Peer stream message types  (Source: client, gateway or peer)
enum stream_msg_type { NULL_MSG_STM, ADD_PHOTO_STM, SEND_PHOTO_STM,
                       SEND_NAME_STM, ADD_KEY_STM, SEND_KEY_STM,
                       GET_NAME_STM, GET_PHOTO_STM, DEL_PHOTO_STM,
                       SRC_PHOTO_STM, SEND_IDS_STM, CONFIRM_STM,
                       FAILED_STM, BEGIN_DOWN, NEXT_PHOTO, END_DOWN};
/*
    type             purpose               direction          id     size
    NULL_MSG_STM     null message          -- -> --           --     --
    ADD_PHOTO_STM    add photo             client -> peer     x      --
    SEND_PHOTO_STM   send photo data       source <-> peer    x      x
    SEND_NAME_STM    send photo name       source <-> peer    x      x
    ADD_KEY_STM      add keyword           client -> peer     x      --
    SEND_KEY_STM     send keyword string   source <-> peer    x      x
    GET_NAME_STM     get photo name        client -> peer     x      --
    GET_PHOTO_STM    get photo             client -> peer     x      --
    DEL_PHOTO_STM    delete photo          client -> peer     x      --
    SRC_PHOTO_STM    search photo          client -> peer     --     x
    SEND_IDS_STM     send Id's             peer -> client     --     --
    CONFIRM_STM      confirmation          peer -> client     x      ??
    FAILED_STM       failure               peer -> client     --     --

    BEGIN_DOWN       begin download        peer -> peer       --     --
    NEXT_PHOTO       next photo            peer -> peer       --     --
    END_DOWN         end download          peer -> peer       --     --
*/

typedef struct message_stream {
  enum stream_msg_type type_m;    // Codifies the message purpose.
  uint32_t id;               // Id of a photo.
  long int size;             // Size of the next transmission.
} message_stream;

#endif
