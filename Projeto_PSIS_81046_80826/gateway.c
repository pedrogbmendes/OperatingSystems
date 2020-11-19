#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include "message.h"
#include "list.h"

#define TIME_SWEEP 21

typedef struct _peer_info {
  struct sockaddr_in addr;
  int port, flag;
  uint32_t peer_id;
} peer_info;

int peers_sock_fd, clients_sock_fd, sock_ping;

list peer_list;

list_node *next_peer;
pthread_mutex_t robin_lock;

void alarm_handler(int sig);
void* serve_peers(void *arg);
void* serve_clients(void *arg);
void * measure_beat(void *arg);

int main() {
  struct sockaddr_in peer_gw_addr, client_gw_addr;

  char port_str[20];
  int peer_gw_port, client_gw_port;
  int err;
  FILE *f_net;

  pthread_t tpeer_id, tclients_id;

  struct sigaction act;

  /* Configures the process termination interruption. */
  act.sa_handler = alarm_handler;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = SA_RESTART;

  sigaction(SIGINT, &act, NULL);

  /* Creates and binds the two gateway sockets. */
  peers_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  clients_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  sock_ping = socket(AF_INET, SOCK_DGRAM, 0);

  f_net = fopen("network_port.txt", "r");


  printf("Peers socket port: ");
  fgets(port_str, 20, f_net);
  sscanf(port_str, "%d", &peer_gw_port);

  printf("Clients socket port: ");
  fgets(port_str, 20, f_net);
  sscanf(port_str, "%d", &client_gw_port);

  peer_gw_addr.sin_family = AF_INET;
  peer_gw_addr.sin_port = htons(peer_gw_port);
  peer_gw_addr.sin_addr.s_addr = INADDR_ANY;

  client_gw_addr.sin_family = AF_INET;
  client_gw_addr.sin_port = htons(client_gw_port);
  client_gw_addr.sin_addr.s_addr = INADDR_ANY;

  fclose(f_net);

  err = bind(peers_sock_fd, (struct sockaddr *) &peer_gw_addr, sizeof(peer_gw_addr));
  if(err == -1) {
    perror("bind, peer sockets1");
    exit(-1);
  }

  err = bind(clients_sock_fd, (struct sockaddr *) &client_gw_addr, sizeof(client_gw_addr));
  if(err == -1) {
    perror("bind, client sockets");
    exit(-1);
  }

  /* Initializes the list of available peers. */
  //peer_info *peer;
  init_list( &peer_list );

  /* Creates two threads, one to handle peers and the other to handle clients.*/
  err = pthread_create(&tpeer_id, NULL, serve_peers, NULL);
  if(err != 0) {
    perror("peers thread");
    exit(-1);
  }

  err = pthread_create(&tclients_id, NULL, serve_clients, NULL);
  if(err != 0) {
    perror("peers thread");
    exit(-1);
  }

  printf("Gateway ready\n");

  /* Waits for termination. */
  while (1) {

  }

  /* Free resources. */
  close(peers_sock_fd);
  close(clients_sock_fd);
  destroy_list( &peer_list, free );

  return 0;
}

void alarm_handler(int sig){

  /* Free resources. */
  close(peers_sock_fd);
  close(clients_sock_fd);
  destroy_list( &peer_list, free );

  exit(1);
}

void * serve_peers(void *arg){
  struct sockaddr_in peer_addr;
  struct sockaddr_in m_peer_addr;
  socklen_t size_addr = sizeof(peer_addr);
  ssize_t m_recv, m_send;
  int err;
  message_gw m_query, m_reply;
  peer_info* new_peer;
  uint32_t peer_id_counter = 0;

  /* Creates a thread to verify if peers are alive. */
  pthread_t t_s_ping;

 err = pthread_create(&t_s_ping, NULL, measure_beat, NULL);
  if (err != 0) {
    perror("Error in thread creation, ping");
    exit(-1);
  }

  while (1) {

    // Recv query.
    m_recv = recvfrom(peers_sock_fd, (void *) &m_query, sizeof(m_query),
                      0, (struct sockaddr *) &peer_addr, &size_addr );
    if(m_recv < 0){
      perror("Peer recv");
      continue;
    }

    if( m_query.type_m == INF_CREAT_PtG ) {
      int port_aux;
      peer_info *master_peer_l;

      int res = 0;

      // Saves peer info.
      port_aux = m_query.port;

      pthread_mutex_lock( &robin_lock );  //RESTRICTED
      if ( next_peer == NULL ) {
        next_peer = get_head( &peer_list );
      } else {
        next_peer = cycle( &peer_list, next_peer );
      }

      if ( next_peer != NULL ) {
        master_peer_l = (peer_info*) get_content( next_peer );
        m_peer_addr = master_peer_l->addr;

        res = 1;
      }
      pthread_mutex_unlock( &robin_lock );  //END RESTRICTED

      if ( res == 0 ) {
        // Reply to peer  - inform about download
        //NO exist more peers - no download
        m_reply.type_m = NO_DOWN_GtP ;

        m_send = sendto(peers_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                        0, (const struct sockaddr *) &peer_addr,
                        sizeof(peer_addr) );
        if(m_send < 0){
          perror("Peer send");
          continue;
        }

      } else {

        // Reply to peer inform about download
        // Exist more peers - download is needed

        m_reply.type_m = INF_DOWN_GtP;
        // message with address and port of master peer

        //reply to slave peer
        m_send = sendto(peers_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                        0, (const struct sockaddr *) &peer_addr,
                        sizeof(peer_addr) );
        if(m_send < 0){
          perror("Peer send");
          continue;
        }

        // message with address and port of slave peer
        strcpy(m_reply.adress, inet_ntoa(peer_addr.sin_addr));
        m_reply.port = port_aux;

        //address of master peer
        m_reply.type_m = REQ_DOWN_GtP ;

        //inform the master peer
        m_send = sendto(peers_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                        0, (const struct sockaddr *) &m_peer_addr,
                        sizeof(m_peer_addr) );
        if(m_send < 0){
          perror("Peer send");
          continue;
        }
      }

    } else if ( m_query.type_m == READY_PtG ) {
      // Peer connecting.
      printf("Peer connecting...");

      // Generate peer id
      peer_id_counter = peer_id_counter + PEER_ID_BASE;

      // Saves peer info.
      new_peer = (peer_info *) malloc( sizeof(peer_info) );
      new_peer->addr = peer_addr;
      new_peer->port =  m_query.port;
      new_peer->peer_id = peer_id_counter;
      new_peer->flag = 1;

      // Reply to peer.
      m_reply.type_m = ACK_CREAT_GtP;
      m_reply.id = peer_id_counter;

      m_send = sendto(peers_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                      0, (const struct sockaddr *) &peer_addr,
                      sizeof(peer_addr) );
      if(m_send < 0){
        perror("Peer send");
        continue;
      }

      pthread_mutex_lock( &robin_lock );  //RESTRICTED
      insert_list( &peer_list, (void *) new_peer );
      pthread_mutex_unlock( &robin_lock );  //END RESTRICTED

      printf("...done\n");

    } else if ( m_query.type_m == PING_GtP ) {
      peer_info *peer;


      pthread_mutex_lock( &robin_lock );  //RESTRICTED
      list_node *next = get_head( &peer_list );

      while( next != NULL ) {

        peer = (peer_info*) get_content(next);

        if ( peer->peer_id == m_query.id ) {
          peer->flag = 1;

          break;
        }

        next = next_list( next );
      }
      pthread_mutex_unlock( &robin_lock );  //END RESTRICTED

    } else {
      printf("Invalid message peer.\n");
      continue;
    }
  }

  return NULL;
}

void * serve_clients(void *arg){
  struct sockaddr_in client_addr;
  socklen_t size_addr = sizeof(client_addr);
  ssize_t  m_recv;
  ssize_t m_send;
  message_gw m_query, m_reply;
  peer_info *peer;
  int res;

  while (1) {

    // Recv query.
    m_recv = recvfrom(clients_sock_fd, (void *) &m_query, sizeof(m_query), 0, (struct sockaddr *) &client_addr, &size_addr );
    if(m_recv < 0){
      perror("recv");
      exit(-1);
    }

    if( m_query.type_m != REQ_PEER_CtG ) {
      printf("Invalid message client.\n");
      continue;
    }

    // Client connecting.
    printf("Client connecting...");

    // Chooses peer.
    res = 0;
    pthread_mutex_lock( &robin_lock );
    if ( next_peer == NULL ) {
      next_peer = get_head( &peer_list );
    } else {
      next_peer = cycle( &peer_list, next_peer );
    }

    if ( next_peer != NULL ) {
      peer = (peer_info*) get_content( next_peer );

      m_reply.port = peer->port;
      strcpy(m_reply.adress, inet_ntoa(peer->addr.sin_addr));

      res = 1;
    }
    pthread_mutex_unlock( &robin_lock );

    if ( res == 1 ) {
      // Give peer.
      m_reply.type_m = GIVE_PEER_GtC;

      // Sends address of peer to connect to.
      m_send = sendto(clients_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                        0, (const struct sockaddr *) &client_addr,
                        sizeof(client_addr) );
      if(m_send < 0){
        perror("sendto");
        continue;
      }

      printf("...done\n");
    } else {
      // Reply no peer available.
      m_reply.type_m = NO_PEER_GtC;

      m_send = sendto(clients_sock_fd, (const void *) &m_reply, sizeof(m_reply),
                      0, (const struct sockaddr *) &client_addr,
                      sizeof(client_addr) );
      if(m_send < 0){
        perror("sendto");
        exit(-1);
      }
      printf("...no peer available\n");
    }

  }

  return NULL;
}

void * measure_beat(void *arg)
{
  list_node *next, *aux;
  peer_info *peer;

  while (1) {

    sleep(TIME_SWEEP);

    /* Verify beat. */
    aux = NULL;
    pthread_mutex_lock( &robin_lock );  //RESTRICTED
    next = get_head( &peer_list );
    while( next != NULL ) {

      peer = (peer_info*) get_content(next);

      if ( peer->flag == 0 ) {

        if ( next_peer == next ) {
          next_peer = next_list(next);
        }

        next = inplace_delete( &peer_list , next , aux , free );

        printf("peer disconnected\n");
      } else {
        peer->flag = 0;

        aux = next;
        next = next_list(next);
      }
    }
    pthread_mutex_unlock( &robin_lock );  //END RESTRICTED

  }

  return NULL;
}
