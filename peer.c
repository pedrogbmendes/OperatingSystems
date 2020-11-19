#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "message.h"
#include "list.h"
#include "photo_data.h"

void alarm_handler( int sig );
void * serve_client( void* arg );
void * accept_clients(void* sock_stream_fd);

int get_new_id();
int cmp_keyword(void*, void*);

void * download_start(void * arg);
int download_slave(int peer_fd);
int download_master(int sock_stream_fd);
int add_photo( int client_fd , uint32_t new_id );
int add_keyword( int client_fd , uint32_t id );
int get_name( int client_fd , uint32_t id );
int get_photo( int client_fd , uint32_t id );
int delete_photo( int client_fd , uint32_t id );
int search_photo( int client_fd );
void* heartbeat( void *arg );

int gw_sock_fd;
int sock_stream_fd;

uint32_t peer_id;

uint32_t id_counter;
pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;

list stored_photos;    /*  */
pthread_rwlock_t list_lock, photo_lock;

int main()
{
  struct sockaddr_in local_stream_addr;
  struct sockaddr_in peer_stream_addr;
  struct sockaddr_in gw_addr, gw_addr1;

  char gw_add[20], gw_port_str[20];
  message_gw m_send_gw, m_recv_gw;
  int gw_port, local_port;
  int err;

  ssize_t m_send, m_recv;
  socklen_t size_gw_addr, size_peer_addr;

  /* Configures interruption handler. */
  struct sigaction act;

  act.sa_handler = alarm_handler;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = SA_RESTART;

  sigaction(SIGINT, &act, NULL);

  /* Initizalize stored photos list. */
  init_list( &stored_photos );

  /* Configure photo repository inter-thread security. */
  err = pthread_rwlock_init( &list_lock , NULL );
  if ( err!= 0 ) {
    perror("List lock init");
    exit(-1);
  }
  err = pthread_rwlock_init( &photo_lock , NULL );
  if ( err!= 0 ) {
    perror("Photo lock init");
    exit(-1);
  }

  /* Datagram Socket to connect with the gateway. */
  gw_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (gw_sock_fd == -1){
		perror("Error creating gateway socket");
		exit(-1);
	}
  FILE *f_net;

  f_net = fopen("network_peer.txt", "rb");

  /* Gets the adress of the gateway. */
  fgets(gw_add, 20, f_net);
  fgets(gw_port_str, 20, f_net);
  sscanf(gw_port_str, "%d", &gw_port);
  fclose(f_net);

  /* Conects the adress to the gateway. */
  gw_addr.sin_family = AF_INET;
  gw_addr.sin_port = htons(gw_port);
  gw_addr.sin_addr.s_addr = inet_addr(gw_add);

  gw_addr1 = gw_addr;
  /* Port. */
  local_port = 3000 + getpid();

  /* Creates stream socket. */
  sock_stream_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_stream_fd == -1){
    perror("Error creating peer socket");
    exit(-1);
  }

  /* Binds the stream socket to a fixed address/port. */
  local_stream_addr.sin_family = AF_INET;
  local_stream_addr.sin_port = htons(local_port);
  local_stream_addr.sin_addr.s_addr = INADDR_ANY;
  err = bind(sock_stream_fd, (struct sockaddr *) &local_stream_addr,
             sizeof(local_stream_addr));
  if(err == -1) {
    if (errno == EADDRINUSE) {
      perror("Error binding: address already used");
    } else {
      perror("Error binding: other");
    }
    exit(-1);
  }

  /* Sends the message to the gateway with the stream socket port adress. */
  m_send_gw.type_m = INF_CREAT_PtG;
  m_send_gw.port = local_port;

  m_send = sendto(gw_sock_fd, (const void *) &m_send_gw, sizeof(m_send_gw), 0,
                  (const struct sockaddr *) &gw_addr, sizeof(gw_addr));
  if(m_send < 0) {
		perror("Error communicating with gateway");
		exit(-1);
	}

  /* Receives the information about download from the gateway, */
  size_gw_addr = sizeof(gw_addr);
  m_recv = recvfrom(gw_sock_fd, (void *) &m_recv_gw, sizeof(m_recv_gw), 0,
                    (struct sockaddr *) &gw_addr, &size_gw_addr );
  if(m_recv < 0){
    perror("Error in the connection between peer and gateway (recvfrom)");
    exit(-1);
  }

  if (m_recv_gw.type_m == NO_DOWN_GtP) {
      // No download is needed

  } else if(m_recv_gw.type_m == INF_DOWN_GtP) {
      // download is needed

      printf("Waiting for master peer...\n");

      /* Waits for a peer to connect. */
      err = listen(sock_stream_fd, 5);
      if (err == -1) {
        if (errno == EADDRINUSE) {
          perror("Error in listen: port already being listenned to");
        } else {
          perror("Error in listen: unknown");
        }
        exit(-1);
      }

      /* Waits for a connection from a peer */
      size_peer_addr = sizeof(peer_stream_addr);
      int peer_fd = accept(sock_stream_fd, (struct sockaddr *) &peer_stream_addr, &size_peer_addr);

      printf("master connected\n");

      err = download_slave(peer_fd);
      if(err == -1){
        perror("download_slave");
      }

      /* End connection */
      close(peer_fd);
  } else {
    /** TODO: ERROR **/
  }

  /* Download complete: peer ready to serve. */
  /* The Local Stream will now be used for clients to connect. */
  m_send_gw.type_m = READY_PtG;
  m_send_gw.port = local_port;

  m_send = sendto(gw_sock_fd, (const void *) &m_send_gw, sizeof(m_send_gw), 0,
                  (const struct sockaddr *) &gw_addr, sizeof(gw_addr));
  if(m_send < 0) {
		perror("Error communicating with gateway");
		exit(-1);
	}

  /* Receives the reply from the gateway, with the peer unique id. */
  size_gw_addr = sizeof(gw_addr);
  m_recv = recvfrom(gw_sock_fd, (void *) &m_recv_gw, sizeof(m_recv_gw), 0,
                    (struct sockaddr *) &gw_addr, &size_gw_addr );

  if(m_recv < 0){
    perror("Error in the connection between peer and gateway (recvfrom)");
    exit(-1);
  }

  if(m_recv_gw.type_m != ACK_CREAT_GtP){
    perror("Error in the communication between peer and gateway");
    exit(-1);
  }

  peer_id = m_recv_gw.id;

  pthread_t tpeer_id;
  struct sockaddr_in gw_heart;
  gw_heart = gw_addr1;

  /* Cretes heartbeat. */
  err = pthread_create(&tpeer_id, NULL, heartbeat, (void*) &gw_heart);
  if(err != 0) {
    perror("peers thread");
    exit(-1);
  }

  /* Creates a thread to accept the client. */
  pthread_t t_acc;
  int *sk1 = (int*) malloc(sizeof(int));
  *sk1 = sock_stream_fd;

  err = pthread_create(&t_acc, NULL, accept_clients, (void*) sk1);
  if (err != 0) {
    perror("Error in thread creation, accept_clients");
    exit(-1);
  }

  /* communication with the gateway: */
  /* wait for replication request or ?ping?. */
  while (1) {

    m_recv = recvfrom(gw_sock_fd, (void *) &m_recv_gw, sizeof(m_recv_gw), 0,
                      (struct sockaddr *) &gw_addr, &size_gw_addr );
    if(m_recv < 0){
      perror("Error in the connection between peer and gateway (recvfrom)");
      exit(-1);
    }

    if( m_recv_gw.type_m == REQ_DOWN_GtP ){

      /* Creates a thread to do the download. */
      pthread_t t_down;
      message_gw *m2 = (message_gw*) malloc(sizeof(message_gw));
      *m2 = m_recv_gw;

      err = pthread_create(&t_down, NULL, download_start, (void*) m2);
      if (err != 0) {
        perror("Error in thread creation, download");
        exit(-1);
      }

    }

  }
  close(gw_sock_fd);
  close(sock_stream_fd);
  return 0;
}

void alarm_handler(int sig)
{
  pthread_rwlock_destroy( &list_lock );
  pthread_rwlock_destroy( &photo_lock );
  destroy_list( &stored_photos , free_photo_data );

  printf("sockets close\n");
  close(gw_sock_fd);
  close(sock_stream_fd);

  exit(1);
}

void* heartbeat( void *arg )
{
  ssize_t m_send;
  struct sockaddr_in *gw_addr = (struct sockaddr_in*) arg;
  message_gw m_send_gw;

  while (1) {

      m_send_gw.type_m = PING_GtP;
      m_send_gw.id = peer_id;

      m_send = sendto(gw_sock_fd, (const void *) &m_send_gw, sizeof(m_send_gw), 0,
                      (const struct sockaddr *) gw_addr, sizeof(struct sockaddr_in));
      if(m_send < 0) {
        perror("Error communicating with gateway");
        exit(-1);
      }

      sleep(5);
  }
}

void * download_start(void * arg)
{
      int sock_master = socket(AF_INET, SOCK_STREAM, 0);
      struct sockaddr_in peer_addr;
      int err;

      message_gw m_recv_gw = *((message_gw*) arg);
      free(arg);

      peer_addr.sin_family = AF_INET;
      peer_addr.sin_port = htons(m_recv_gw.port);
      inet_aton(m_recv_gw.adress, &peer_addr.sin_addr);

      /* Makes the connection with the server. */
      err = connect(sock_master, (struct sockaddr *) &peer_addr, sizeof(peer_addr));
      if(err == -1){
        perror("fail connect between peers");
        return NULL;
      }

      printf("connect to slave\n");

      err = download_master(sock_master);
      if(err == -1){
        //ERROR conne killed
      }

      close(sock_master);
      return NULL;
}

int download_slave(int peer_fd){
  message_stream recv_m;
  ssize_t m_recv, rem_bytes;
  char *file_name1;
  char *data, *key;
  FILE *fp;
  char *buff;

  //recive the information begin download
  m_recv = read( peer_fd, (void *) &recv_m, sizeof(recv_m) );
  if(m_recv < 0) {
    perror("read size of file_name");
    exit(-1);
  }

  if(recv_m.type_m != BEGIN_DOWN){
    return -1;
  }

  while(recv_m.type_m != END_DOWN){
    //Recive the size of the file_name
    m_recv = read( peer_fd, (void *) &recv_m, sizeof(recv_m) );
    if(m_recv < 0) {
      perror("read size of file_name");
      exit(-1);
    }

    if( recv_m.type_m != END_DOWN){

      photo_data *photo = (photo_data*)malloc(sizeof(photo_data));

      if( recv_m.type_m != SEND_NAME_STM){
        return-1;
      }

      photo->id = recv_m.id;
      file_name1 = (char*)malloc( recv_m.size * sizeof(char) );

      //Recive the file_name
      m_recv = read( peer_fd, (void *) file_name1, recv_m.size * sizeof(char) );
      if(m_recv < 0) {
        perror("read size of file_name");
        exit(-1);
      }
      photo->file_name = file_name1;


      //Recive the size of the photo
      m_recv = read( peer_fd, (void *) &recv_m, sizeof(recv_m) );
      if(m_recv < 0) {
        perror("read size of file_name");
        exit(-1);
      }
      if( recv_m.type_m != SEND_PHOTO_STM){
          printf("SEND_PHOTO_STM\n");
        return-1;
      }

      photo->size_photo = recv_m.size;

      buff = data = (char*) malloc( recv_m.size * sizeof(char));
      rem_bytes = recv_m.size * sizeof(char);

      // Recive the photo
      while( rem_bytes > 0 ) {
        m_recv = read( peer_fd, (void *) buff,  rem_bytes* sizeof(char));
        if(m_recv < 0) {
          perror("read size of file_name");
          exit(-1);
        }

        rem_bytes = rem_bytes - m_recv;
        buff = buff + m_recv;
      }

      sprintf( (photo->file_tag) , "%08x" PRIx32 ".jpg", (photo->id) );

      fp = fopen( photo->file_tag , "wb");
      if(fp == NULL){
        perror("Error in open the file : invalid file_name");
        return -1;
      }
      /* writing of the file */
      fwrite(data, photo->size_photo, 1 , fp);
      fclose(fp);
      free(data);


      init_list( &(photo->keywords) );

      while(recv_m.type_m != NEXT_PHOTO){

        m_recv = read( peer_fd, (void *) &recv_m, sizeof(recv_m) );
        if(m_recv < 0) {
          perror("read size of file_name");
          exit(-1);
        }

        if(recv_m.type_m == NEXT_PHOTO){
          //NO KEYWORDS

        }else if(recv_m.type_m == SEND_KEY_STM){

          key = (char*)malloc(recv_m.size * sizeof(char));


          //Recive the keyword
          m_recv = read( peer_fd, (void *) key, (recv_m.size * sizeof(char) ) );
          if(m_recv < 0) {
            perror("read size of file_name");
            exit(-1);
          }

          insert_list( &(photo->keywords), (void*) key);

        }
      }

      insert_list(&stored_photos, (void *) photo);
    }

  }

return 0;

}

int download_master(int sock_stream_fd){
  list_node *aux = NULL;
  ssize_t nbytes_w;
  message_stream m_stream;
  photo_data *data;
  char *photo_data;
  FILE *fp;

  //inform the begining of download
  m_stream.type_m = BEGIN_DOWN;

  nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - CONFIRM_STM - socket (stream)");
    return -1;
  }

  pthread_rwlock_rdlock( &list_lock );  //RESTRICTED_LIST
  aux = get_head(&stored_photos);
  while( aux != NULL ){

    data = get_content(aux);

    //Sends the size of the file_name
    m_stream.type_m = SEND_NAME_STM;
    m_stream.size = strlen(data->file_name)+1;
    m_stream.id = data->id;

    nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
    if(nbytes_w < 0) {
      perror("Error in the communication peer->peer - send size filename");
      return -1;
    }

    //Sends the file_name
    nbytes_w = write( sock_stream_fd , (void *) data->file_name, m_stream.size*sizeof(char) );
    if(nbytes_w < 0) {
      perror("Error in the communication peer->peer - send filename");
      return -1;
    }

    //Sends the size of the photo
    m_stream.type_m = SEND_PHOTO_STM;
    m_stream.size = data->size_photo;
    m_stream.id = data->id;

    photo_data = (char*)malloc(data->size_photo*sizeof(char));


    fp = fopen( data->file_tag, "rb");
    if(fp == NULL){
      perror("Error in open the file : invalid file_name");
      return -1;
    }
    fread( photo_data, data->size_photo, 1 , fp);


    nbytes_w = write( sock_stream_fd , (void*) &m_stream, sizeof(m_stream) );
    if(nbytes_w < 0) {
      perror("Error in the communication peer->peer - send size photo");
      return -1;
    }


    // Sends photo
    nbytes_w = write( sock_stream_fd , (void *) photo_data, (data->size_photo)*sizeof(char) );
    if(nbytes_w < 0) {
      perror("Error in the communication peer->peer - send photo");
      return -1;
    }
    free(photo_data);



    pthread_rwlock_rdlock( &photo_lock );  // RESTRICTED_PHOTO
    list_node *list_key = get_head( &(data->keywords) );
    char *node_l = NULL;

    if ( list_key == NULL ) {
      //NO keywords

      m_stream.type_m = NEXT_PHOTO;
      nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
      if(nbytes_w < 0) {
        perror("Error in the communication peer->peer - send size of keyword");
        return -1;
      }

    } else {

      while( list_key != NULL){

        node_l = (char*) get_content(list_key);

        m_stream.type_m = SEND_KEY_STM;
        m_stream.size = strlen(node_l)+1;
        m_stream.id = data->id;

        //Sends the size of keyword
        nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
        if(nbytes_w < 0) {
          perror("Error in the communication peer->peer - send size of keyword");
          return -1;
        }

        //Sends the keyword
        nbytes_w = write( sock_stream_fd , (void *) node_l, m_stream.size*sizeof(char) );
        if(nbytes_w < 0) {
          perror("Error in the communication peer-peer - send keyword");
          exit(-1);
        }
        list_key = next_list(list_key);
      }

      m_stream.type_m = NEXT_PHOTO;
      //Sends the inform about next photo
      nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
      if(nbytes_w < 0) {
        perror("Error in the communication peer->peer - next photo");
        return -1;
      }
    }
    pthread_rwlock_unlock( &photo_lock );  // END RESTRICTED_PHOTO

    aux = next_list(aux);
  }
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED_LIST

  m_stream.type_m = END_DOWN;
  //End download
  nbytes_w = write( sock_stream_fd , (void *) &m_stream, sizeof(m_stream) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->peer - next photo");
    return -1;
  }

  return 0;
}

void *accept_clients(void* arg)
{
  int err;
  struct sockaddr_in client_stream_addr;
  socklen_t size_client_addr;
  int sock_stream_fd = *( (int*) arg);
  free(arg);

  printf("Waiting for client...\n");

  /* Waits for a client to connect. */
  err = listen(sock_stream_fd, 5);
  if (err == -1) {
    if (errno == EADDRINUSE) {
      perror("Error in listen: port already being listenned to");
    } else {
      perror("Error in listen: unknown");
    }

    exit(-1);
  }

  while (1) {
    /* Waits for a connection from a client. */
    size_client_addr = sizeof(client_stream_addr);
    int client_fd = accept(sock_stream_fd, (struct sockaddr *) &client_stream_addr, &size_client_addr);

    printf("Client connected\n");

    /* Creates a thread to handle the client. */
    pthread_t t_id;
    int *sk = (int*) malloc(sizeof(int));
    *sk = client_fd;

    err = pthread_create(&t_id, NULL, serve_client, (void*) sk);
    if (err != 0) {
      perror("Error in thread creation, serve_client");
      exit(-1);
    }
  }
}

void *serve_client(void * arg)
{
  message_stream query, reply;
  ssize_t nbytes_w, nbytes_r;
  int client_fd = *((int *) arg);

  free(arg);

  while(1) {
    /* Receives a query from the client. */
    nbytes_r = read( client_fd, (void *) &query, sizeof(query) );
    if(nbytes_r < 0) {
      /* Error with connection. */

      perror("Error reading from client");
      exit(-1);

    } else if(nbytes_r == 0) {
      /* Client closed. */

      printf("Client disconnected\n");
      close( client_fd );

      return NULL;
    } else {
      int res;
      int new_id;

      /* Received query. */
      switch ( query.type_m ) {
        case ADD_PHOTO_STM:
          new_id = get_new_id();

          res = add_photo( client_fd , new_id );
          break;

        case ADD_KEY_STM:
          res = add_keyword( client_fd , query.id );
          break;

        case GET_NAME_STM:
          res = get_name( client_fd , query.id );
          break;

        case GET_PHOTO_STM:
          res = get_photo( client_fd , query.id );
          break;

        case DEL_PHOTO_STM:
          res = delete_photo( client_fd , query.id );
          break;

        case SRC_PHOTO_STM:
          res = search_photo( client_fd );
          break;

        default:
          /* Invalid message type. */
          perror("Invalid message received from client. Connection broken");
          close(client_fd);

          return NULL;
      }

      if ( res == -1 ) {
        /* Problem with the connection (socket fail or invalid message). */
        perror("Connection broken");
        close(client_fd);

        return NULL;
      } else if ( res == 0 ) {
        /* Failure with the query. Reply to client. */
        reply.type_m = FAILED_STM;

        nbytes_w = write( client_fd , (void *) &reply, sizeof(reply) );
        if(nbytes_w < 0) {
          perror("Error in the communication peer->client (write_peer - CONFIRM_STM - socket (stream)");
          exit(-1);
        }
      }

    }
  }

  return NULL;
}

int get_new_id ()
{
  uint32_t new_id;

  /* Generates a new unique id, using the peer_id */
  pthread_mutex_lock( &id_lock );   // RESTRICTED
  id_counter = id_counter+1;
  new_id = id_counter;
  pthread_mutex_unlock( &id_lock );   // END RESTRICTED

  new_id = new_id + peer_id;

  return new_id;
}

int cmp_keyword(void *key1, void *key2)
{
  if ( strcmp( (const char*) key1 , (const char*) key2 ) == 0 ) {
    return 1;
  } else {
    return 0;
  }
}

int add_photo( int client_fd , uint32_t new_id )
{
  ssize_t nbytes_w, nbytes_r, rem_bytes;
  photo_data *new_photo;
  message_stream query, reply;
  long int size_name, size_photo;
  char *photo, *buff;
  FILE *fp;
  /* TODO: Check if id is valid ??? return 0 ??? */

  /* Creates new photo. */
  new_photo = new_photo_data( new_id );

  /* TODO: add locks; choose order */

  /* Sends confirmation to client. */
  reply.type_m = CONFIRM_STM;
  reply.id = new_id;

  nbytes_w = write( client_fd , (void *) &reply, sizeof(reply) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - CONFIRM_STM - socket (stream)");
    exit(-1);
  }

  /* Completes photo data: */

  /* Receives name from client. */
  nbytes_r = read( client_fd, (void *) &query, sizeof(query) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    exit(-1);
  }

  if ( query.type_m != SEND_NAME_STM || query.id != new_id ) {

    perror("Error in the communication client->peer (read_peer - SEND_NAME_STM - socket (stream)");
    return -1;
  }

  size_name = query.size;

  new_photo->file_name = (char*) calloc( size_name , sizeof(char) );

  nbytes_r = read( client_fd, (void *) (new_photo->file_name),
                   size_name * sizeof(char) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  /* Receives photo data. */
  nbytes_r = read( client_fd, (void *) &query, sizeof(query) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  if ( query.type_m != SEND_PHOTO_STM || query.id != new_id ) {
    perror("Error in the communication client->peer (read_peer - SEND_PHOTO_STM - socket (stream)");
    return -1;
  }

  size_photo = query.size;

  buff = photo = (char*) calloc( size_photo , sizeof(char) );
  new_photo->size_photo = size_photo;
  rem_bytes = size_photo * sizeof(char);

  while (rem_bytes > 0){
    nbytes_r = read( client_fd, (void *) (buff),
                    rem_bytes * sizeof(char) );
    if(nbytes_r < 0) {
      /* Error with connection. */

      perror("Error reading from client");
      return -1;
    }

    rem_bytes = rem_bytes - nbytes_r;
    buff = buff + nbytes_r;
  }

  sprintf(new_photo->file_tag, "%08x" PRIx32".jpg",new_id);


  fp = fopen( new_photo->file_tag , "wb");
  if(fp == NULL){
    perror("Error in open the file : invalid file_name");
    return -1;
  }

  /* writing of the file */
  fwrite(photo, size_photo, 1 , fp);
  fclose(fp);



  /* Inserts photo in list (only when no more data will be changed). */
  pthread_rwlock_wrlock( &list_lock );  //RESTRICTED LIST
  insert_list ( &stored_photos , (void*) new_photo );
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED LIST

  return 1;
}

int add_keyword( int client_fd , uint32_t id )
{
  ssize_t nbytes_w, nbytes_r;
  message_stream query;
  long int size_key;
  list_node *aux;
  char* keyword;
  photo_data *search;
  int res = 0;

  /* Receive keyword. */
  nbytes_r = read( client_fd, (void *) &query, sizeof(query) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  if ( query.type_m != SEND_KEY_STM || query.id != id ) {
    perror("Error in the communication client->peer (read_peer - SEND_KEY_STM - socket (stream)");
    return -1;
  }

  size_key = query.size;
  keyword = (char*) calloc( size_key , sizeof(char) );

  nbytes_r = read( client_fd, (void *) keyword, size_key * sizeof(char) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  /* TODO: add locks; choose order */
  /* Find photo, if it exists. */
  pthread_rwlock_rdlock( &list_lock );  //RESTRICTED_LIST
  aux = get_head( &stored_photos );
  while ( aux != NULL ) {
    search = (photo_data*) get_content( aux );

    if ( search->id == id ) {
      /* Add keyword to photo. ?RESTRICTED? */
      pthread_rwlock_wrlock( &photo_lock );  // RESTRICTED_PHOTO
      insert_list( &(search->keywords) , (void*) keyword );
      pthread_rwlock_unlock( &photo_lock );  // END RESTRICTED_PHOTO

      res = 1;
      break;
    }

    aux = next_list( aux );
  }
  pthread_rwlock_unlock( &list_lock );  //RESTRICTED_LIST

  if ( res == 0 ) {
    /* FAILURE: photo does not exist in store. */
    return 0;
  }

  query.type_m = CONFIRM_STM;
  nbytes_w = write(client_fd, (void *) &query, sizeof(query));
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - CONFIRM_STM)- socket (stream)");
    return 0;
  }

  return 1;
}

int get_name( int client_fd , uint32_t id )
{
  ssize_t nbytes_w;
  photo_data *search;
  message_stream reply;
  long int size_name;
  list_node *aux;
  char* keyword;
  int res = 0;

  /* TODO: add locks; choose order */
  /* Find photo, if it exists. */
  pthread_rwlock_rdlock( &list_lock );  //RESTRICTED_LIST
  aux = get_head( &stored_photos );
  while ( aux != NULL ) {
    search = (photo_data*) get_content( aux );

    if ( search->id == id ) {

      size_name = strlen(search->file_name) + 1;

      keyword = (char*) malloc(size_name * sizeof(char));
      strcpy(keyword, search->file_name);
      keyword[size_name] = '\0';

      res = 1;
      break;
    }

    aux = next_list( aux );
  }
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED_LIST

  if ( res == 0 ) {
    /* FAILURE: photo does not exist in store. */
    return 0;
  }

  reply.type_m = SEND_NAME_STM;
  reply.size = size_name;
  reply.id = id;

  nbytes_w = write(client_fd, (void *) &reply, sizeof(reply));
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - inform client)- SEND_NAME_STM - socket (stream)");
    return -1;
  }

  nbytes_w = write(client_fd, (char*) keyword, size_name * sizeof(char) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - send name) - SEND_NAME_STM - socket (stream)");
    return -1;
  }

  free(keyword);
  return 1;
}

int get_photo( int client_fd , uint32_t id )
{
  ssize_t nbytes_w;
  photo_data *search;
  message_stream reply;
  long int size_photo;
  list_node *aux;
  char *photo_donwload;
  int res = 0;
  FILE* fp;
  char file_tag[20];


  /* TODO: add locks; choose order */
  /* Find photo, if it exists. */
  pthread_rwlock_rdlock( &list_lock );  //RESTRICTED_LIST
  aux = get_head( &stored_photos );
  while ( aux != NULL ) {
    search = (photo_data*) get_content( aux );

    if ( search->id == id ) {
      size_photo=search->size_photo;
        strcpy(file_tag, search->file_tag);
      res = 1;

      break;
    }

    aux = next_list( aux );
  }
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED_LIST

  if ( res == 0 ) {
    /* FAILURE: photo does not exist in store. */
    return 0;
  }

  /* Reads photo from file. TODO */
  photo_donwload = (char *)malloc( (size_photo * sizeof(char)) );
  if(photo_donwload == NULL){
    perror("Error in the allocation of the memory - photo's string");
    return -1;
  }
  fp = fopen(file_tag, "rb");
  if(fp == NULL){
    perror("Error in open the file : invalid file_name");
    return -1;
  }

  /* writing of the file */
  fread(photo_donwload, size_photo, 1 , fp);

  fclose(fp);


  /* Sends photo. */
  reply.type_m = SEND_PHOTO_STM;
  reply.size = size_photo;

  reply.id = id;

  nbytes_w = write(client_fd, (void *) &reply, sizeof(reply));
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - inform client)- SEND_PHOTO_STM - socket (stream)");
    return -1;
  }

  nbytes_w = write(client_fd, (char *) photo_donwload, size_photo * sizeof(char) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - send name) - SEND_PHOTO_STM - socket (stream)");
    return -1;
  }
  free(photo_donwload);
  return 1;
}

int delete_photo( int client_fd , uint32_t id )
{
  photo_data *search;
  message_stream reply;
  list_node *aux;
  ssize_t nbytes_w;
  int res = 0;

  /* TODO: add locks; choose order */
  /* Find photo, if it exists. *RESTRICTED* */
  pthread_rwlock_wrlock( &list_lock );  //RESTRICTED LIST
  aux = get_head( &stored_photos );

  while ( aux != NULL ) {
    search = (photo_data*) get_content( aux );

    if ( search->id == id ) {
      /* FIXME: delete photo: locks; order; state. */
      /* Remove from list. *RESTRICTED*. */
      delete_node(&stored_photos, aux , free_photo_data);
      /* *END RESTRICTED* */
      /* Delete photo data. */

      res = 1;
      break;
    }

    aux = next_list( aux );
  }
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED LIST

  if ( res == 0 ) {

    /* FAILURE: photo does not exist in store. */
    return 0;
  }

  reply.type_m = CONFIRM_STM;
  reply.id = id;
  nbytes_w = write(client_fd, (void *) &reply, sizeof(reply) );
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer) - Photos");
    return -1;
  }

  return 1;
}

int search_photo( int client_fd )
{
  ssize_t nbytes_w, nbytes_r;
  photo_data *search;
  message_stream query, reply;
  long int size_key, n_ids;
  list_node *aux, *aux2;
  char *keyword;
  list res_search;
  uint32_t *res_ids, *new_id;
  int res;

  /* Receive keyword. */
  nbytes_r = read( client_fd, (void *) &query, sizeof(query) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  if ( query.type_m != SEND_KEY_STM ) {
    perror("Error in the communication client->peer (read_peer - SEND_KEY_STM - socket (stream)");
    return -1;
  }

  size_key = query.size;
  keyword = (char*) calloc( size_key , sizeof(char) );

  nbytes_r = read( client_fd, (void *) keyword, size_key * sizeof(char) );
  if(nbytes_r < 0) {
    /* Error with connection. */

    perror("Error reading from client");
    return -1;
  }

  init_list( &res_search );
  /* TODO: add locks; choose order; convert to array */
  /* Search in each photo's keyword list. */
  pthread_rwlock_rdlock( &list_lock );  //RESTRICTED_LIST
  aux = get_head( &stored_photos );
  n_ids = 0;
  while ( aux != NULL ) {
    search = (photo_data*) get_content( aux );

    pthread_rwlock_rdlock( &photo_lock );  // RESTRICTED_PHOTO
    res = contains_list( &(search->keywords), (void*) keyword, cmp_keyword );
    pthread_rwlock_unlock( &photo_lock );  // END RESTRICTED_PHOTO

    if ( res == 1 ) {
      new_id = (uint32_t*) calloc(1, sizeof(uint32_t));
      (*new_id) = search->id;

      insert_list( &res_search , (void*) new_id );
      n_ids++;
    }

    aux = next_list( aux );
  }
  pthread_rwlock_unlock( &list_lock );  //END RESTRICTED_LIST

  /* Send search result to client. */
  reply.type_m = SEND_IDS_STM;
  reply.size = n_ids;

  nbytes_w = write(client_fd, (void *) &reply, sizeof(reply));
  if(nbytes_w < 0) {
    perror("Error in the communication peer->client (write_peer - inform client)- SEND_IDS_STM - socket (stream)");
    return -1;
  }

  if ( n_ids > 0 ) {
    /* If the result is not null. */

    /* Create an array with the ids. */
    res_ids = (uint32_t*) calloc( n_ids , sizeof(uint32_t) );
    aux2 = get_head( &res_search );
    for ( int i = 0 ; i < n_ids ; i++ ) {
      res_ids[i] = *((int*) get_content( aux2 ));

      aux2 = next_list( aux2 );
    }
    destroy_list( &res_search , free );

    /* Send ids. */
    nbytes_w = write( client_fd, (uint32_t*) res_ids,
                      n_ids * sizeof(uint32_t) );
    if(nbytes_w < 0) {
      perror("Error in the communication peer->client (write_peer - send name) - SEND_IDS_STM - socket (stream)");
      return -1;
    }

    free( res_ids );
  }

  return 1;
}
