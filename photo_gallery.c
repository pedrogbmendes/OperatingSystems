#include "photo_gallery.h"



/*=============================================================================
* int gallery_connect(char *host, in_port_t port)
*
* Arguments:
*             - host - address of the gateway
*             - port - port of the gateway
*
* Return Values:
*             - return (int) sock_stream_fd - identification of the socket
*             - return  0 if no peer is available
*             - return -1 in case of error
*             - in case  return 0
*
*=============================================================================*/


int gallery_connect(char *host, in_port_t port)
{

  struct sockaddr_in gw_addr;
  struct sockaddr_in peer_addr;

  message_gw m_send_gw, m_recv_gw;

  ssize_t m_send, m_recv;

  socklen_t size_client_addr;

  /* Datagram Socket to connect with the gateway. */
  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd == -1){
    perror("Error in the creation of the socket (Datagram) on client");
    exit(-1);
  }

  /* Connects the adress to the gateway. */
  gw_addr.sin_family = AF_INET;
  gw_addr.sin_port = htons(port);
  inet_aton(host, &gw_addr.sin_addr);

  /* Sends a message to tha gateway to request an peer's address. */
  m_send_gw.type_m = REQ_PEER_CtG;

  m_send = sendto(sock_fd, (const void *) &m_send_gw, sizeof(m_send_gw), 0,
  (const struct sockaddr *) &gw_addr, sizeof(gw_addr));

  if(m_send < 0) {
    perror("Errror in the connection between client and gateway (sendto)");
    return -1;
  }

  /* Gets reply from the gateway with the peer's address */
  size_client_addr = sizeof(gw_addr);

  m_recv = recvfrom(sock_fd, (void *) &m_recv_gw, sizeof(m_recv_gw), 0,
  (struct sockaddr *) &gw_addr, &size_client_addr );

  if(m_recv < 0){
    perror("Error in the connection between client and gateway (recvfrom)");
    return-1;
  }

  if(m_recv_gw.type_m == NO_PEER_GtC){
    return 0;
  }

  if(m_recv_gw.type_m == GIVE_PEER_GtC){

    /* Stream Socket to connect with a peer. */
    int sock_stream_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_stream_fd == -1){
      perror("Error in the creation of the socket (stream) on client");
      exit(-1);
    }

    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(m_recv_gw.port);
    inet_aton(m_recv_gw.adress, &peer_addr.sin_addr);

    /* Makes the connection with the server. */
    connect(sock_stream_fd, (struct sockaddr *) &peer_addr, sizeof(peer_addr));

    return sock_stream_fd;
  }
}




/*=============================================================================
* uint32_t gallery_add_photo(int peer_socket, char *file_name)
*
* Arguments:
*             - peer_socket - int corresponding to the socket
*             - file_name   - name of the photo file
*
* Return Values:
*             - return id
*             - in case of error return 0
*
*=============================================================================*/

uint32_t gallery_add_photo(int peer_socket, char *file_name)
{

  message_stream m_add_photo, m_photo;
  char *photo;
  ssize_t nbytes_w, nbytes_r;
  uint32_t id;
  long int size;

  /* Read the file */
  size = read_file(file_name, &photo);
  if(size == 0){
    return 0;
  }

  /* Informs the peer that the client wants to add a new photo */
  m_add_photo.type_m = ADD_PHOTO_STM;
  nbytes_w = write(peer_socket, (void *) &m_add_photo, sizeof(m_add_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer) - ADD_PHOTO_STM - socket (stream)");
    return 0;
  }

  /* Recive the photo id from the peer */
  nbytes_r = read(peer_socket, (void *) &m_photo, sizeof(m_photo) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive id) - ADD_PHOTO_STM - socket (stream)");
    return 0;
  }

  if ( m_photo.type_m == FAILED_STM ) {
    /* FIXME */
    return 0;
  } else if ( m_photo.type_m != CONFIRM_STM ) {
    perror("Invalid message type peer->client (read_client - receive confirmation) - ADD_PHOTO_STM");
    return 0;
  }

  id = m_photo.id;

  /* Send the file name to the peer*/
  m_photo.size = strlen(file_name) + 1;
  m_photo.type_m = SEND_NAME_STM;
  m_photo.id = id;

  nbytes_w = write(peer_socket, (void *) &m_photo, sizeof(m_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- SEND_NAME_STM - socket (stream)");
    return 0;
  }

  nbytes_w = write(peer_socket, (void *) file_name, ( (strlen(file_name) + 1)
  * sizeof(char) ) );
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - send name) - SEND_NAME_STM - socket (stream)");
    return 0;
  }


  /* Send the photo to the peer*/
  m_photo.size = size;
  m_photo.type_m = SEND_PHOTO_STM;
  m_photo.id = id;

  nbytes_w = write(peer_socket, (void *) &m_photo, sizeof(m_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer) - SEND_PHOTO_STM - socket (stream)");
    return 0;
  }

  nbytes_w = write(peer_socket, (void *) photo, ( ( sizeof(char) ) * size) );
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - send photo) - SEND_PHOTO_STM - socket (stream)");
    return 0;
  }

  return id;
}



/*=============================================================================
* int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword)
*
* Arguments:
*             - peer_socket - int corresponding to the socket
*             - id_photo    - id (identification) of the photo
*             - keyword     - keyword to add to the photo
*
* Return Values:
*             - return
*             - in case of error return
*
*=============================================================================*/

int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword)
{

  message_stream m_send_photo, m_recv_photo;
  ssize_t nbytes_w, nbytes_r;

  m_send_photo.id = id_photo;

  /* Inform the peer that client wants to add a keyword */
  m_send_photo.type_m = ADD_KEY_STM;

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- ADD_KEY_STM - socket (stream)");
    return 0;
  }

  /* Send the keyword to the peer */
  m_send_photo.id = id_photo;
  m_send_photo.type_m = SEND_KEY_STM;
  m_send_photo.size = strlen(keyword);

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- SEND_KEY_STM - socket (stream)");
    return 0;
  }

  nbytes_w = write(peer_socket, (void *) keyword, ( strlen(keyword)
  * sizeof(char) ));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - send name) - ADD_KEY_STM - socket (stream)");
    return 0;
  }


  nbytes_r = read(peer_socket, (void *) &m_recv_photo, sizeof(m_recv_photo) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive id) - ADD_KEY_STM - socket (stream)");
    return 0;
  }

  if( m_recv_photo.type_m == CONFIRM_STM  ){
    return 1;
  }

  return 0;
}




/*=============================================================================
* int gallery_search_photo(int peer_socket, char *keyword, uint32_t **id_photos)
*
* Arguments:
*             - peer_socket - int corresponding to the socket
*             - keyword     - keyword to search for the photo
*             - **id_photos - pointer to an array of id's
*
* Return Values:
*             - return n_id - number of photos with the keyword
*             - in case of error return -1
*
*=============================================================================*/

int gallery_search_photo(int peer_socket, char *keyword, uint32_t **id_photos)
{

  message_stream m_send_photo, m_recv_photo;
  ssize_t nbytes_w, nbytes_r;
  long int n_id;
  void *data;

  m_send_photo.size = strlen(keyword);

  /* Inform the peer that client wants to search a photo with a keyword */
  m_send_photo.type_m = SRC_PHOTO_STM  ;

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- SEARCH_PHOTO - socket (stream)");
    return -1;
  }

  m_send_photo.type_m = SEND_KEY_STM;

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- SEARCH_PHOTO - socket (stream)");
    return -1;
  }
  nbytes_w = write(peer_socket, (void *) keyword, m_send_photo.size * sizeof(char));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- SEARCH_PHOTO - socket (stream)");
    return -1;
  }

  /* Recive the information about how many photos have the keyword */
  nbytes_r = read(peer_socket, (void *) &m_recv_photo, sizeof(m_recv_photo) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive number of photos) - SEARCH_PHOTO - socket (stream)");
    return -1;
  }

  n_id = m_recv_photo.size;

  /* n_id correspond to the numbers os photos with the keyword
      if doesn't exist photos -> n_id = 0 and return 0
      if an error occurs -> n_id = -1 and return -1
      if there are n_id photos (with n_id > 0) return n_id
  */
  if(n_id == 0){
    return 0;
  }else if(n_id == -1){
    return-1;
  }else if(n_id > 0){

    data = (uint32_t*) calloc( n_id, sizeof(uint32_t) );
    if(data == NULL){
      perror("Error in the allocation of the memory - array of id_photos");
      return -1;
    }


    /* Recive de array of id's */
    nbytes_r = read(peer_socket, (void*) data, n_id*sizeof(uint32_t) );
    if(nbytes_r < 0) {
      perror("Error in the communication peer->client (read_client - recive array of id's) - SEARCH_PHOTO - socket (stream)");
      return -1;
    }

    *id_photos =  data;
    return n_id;
  }

  return -1;
}



/*=============================================================================
* int gallery_delete_photo(int peer_socket, uint32_t id_photo)
*
* Arguments:
*             - peer_socket - int corresponding to the socket
*             - id_photo    - id (identification) of the photo
*
* Return Values:
*             - return 1 if the id_photo was removed successfully
*             - return 0 if the photo doesn't exist
*             - return -1 in case of error
*
*=============================================================================*/

int gallery_delete_photo(int peer_socket, uint32_t id_photo)
{

    message_stream m_send_photo, m_recv_photo;
    ssize_t nbytes_w, nbytes_r;

    /* Inform the peer that client wants to delete a photo with the id_photo */
    m_send_photo.type_m = DEL_PHOTO_STM;
    m_send_photo.id = id_photo;

    nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
    if(nbytes_w < 0) {
      perror("Error in the communication client->peer (write_client - inform peer)- DEL_PHOTO_STM - socket (stream)");
      return -1;
    }

    /* Recive the information if the photo was removed successfully or
    if the photo does not exist */
    nbytes_r = read(peer_socket, (void *) &m_recv_photo, sizeof(m_recv_photo) );
    if(nbytes_r < 0) {
      perror("Error in the communication peer->client (read_client - recive number of photos) - DEL_PHOTO_STM - socket (stream)");
      return -1;
    }

    /* if the photo was removed successfully -> m_recv_photo.id == id_photo
       if the photo doesn't exist -> m_recv_photo.id  = 0  */
    if(m_recv_photo.type_m == CONFIRM_STM ){
      return 1;
    }else if(m_recv_photo.type_m == FAILED_STM){
      return 0;
    }

    return -1;
}


/*=============================================================================
* int gallery_get_photo_name(int peer_socket, uint32_t id_photo,
*       char **photo_name)
*
* Arguments:
*             - peer_socket  - int corresponding to the socket
*             - id_photo     - id (identification) of the photo
*             - **photo_name - pointer to the string with the photo_name
*
* Return Values:
*             - return 1 if the photo existes in the system
*       and the name was retrieved
*             - return 0 if the photo doesn't exist
*             - return -1 in case of error
*
*=============================================================================*/

int gallery_get_photo_name(int peer_socket, uint32_t id_photo, char **photo_name)
{

  message_stream m_send_photo, m_recv_photo;
  ssize_t nbytes_w, nbytes_r;
  long int size_name;

  /* Inform the peer that client wants the name of the photo with the id_photo */
  m_send_photo.type_m = GET_NAME_STM ;
  m_send_photo.id = id_photo;

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- GET_NAME_STM  - socket (stream)");
    return -1;
  }

  /* Recive the information of the size of the photo_name */
  nbytes_r = read(peer_socket, (void *) &m_recv_photo, sizeof(m_recv_photo) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive the size of photo_name) - GET_NAME_STM - socket (stream)");
    return -1;
  }

  size_name = m_recv_photo.size;

  /* if the photo doesn't exist size_name = 0 */
  if(size_name == 0){
    return 0;
  }

  *photo_name = (char*)calloc(size_name, sizeof(char));
  if(photo_name == NULL){
    perror("Error in the allocation of the memory - photo_name's string");
    return -1;
  }

  /* Recive the photo_name */
  nbytes_r = read(peer_socket, (void *) *photo_name, size_name * sizeof(char) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive the photo_name) - GET_NAME_STM - socket (stream)");
    return -1;
  }
  return 1;
}


/*=============================================================================
* int gallery_get_photo(int peer_socket, uint32_t id_photo, char *file_name)
*
* Arguments:
*             - peer_socket  - int corresponding to the socket
*             - id_photo     - id (identification) of the photo
*             - *file_name   - name of the file that will contain the image
*       downloaded from the system
*
* Return Values:
*             - return 1 if the photo is downloaded successfully
*             - return 0 if the photo doesn't exist
*             - return -1 in case of error
*
*=============================================================================*/

int gallery_get_photo(int peer_socket, uint32_t id_photo, char *file_name)
{

  message_stream m_send_photo, m_recv_photo;
  ssize_t nbytes_w, nbytes_r;
  long int size_photo;
  char *photo, *buff;
  FILE *fp;
  ssize_t rem_bytes;

  /* Inform the peer that client wants to download the id_photo*/
  m_send_photo.type_m = GET_PHOTO_STM  ;
  m_send_photo.id = id_photo;

  nbytes_w = write(peer_socket, (void *) &m_send_photo, sizeof(m_send_photo));
  if(nbytes_w < 0) {
    perror("Error in the communication client->peer (write_client - inform peer)- GET_PHOTO_STM  - socket (stream)");
    return -1;
  }

  /* Recive the information of the size of the name */
  nbytes_r = read(peer_socket, (void *) &m_recv_photo, sizeof(m_recv_photo) );
  if(nbytes_r < 0) {
    perror("Error in the communication peer->client (read_client - recive the size of photo) - GET_PHOTO_STM - socket (stream)");
    return -1;
  }

  size_photo = m_recv_photo.size;

  /* if the photo doesn't exist size_photo = 0 */
  if(size_photo == 0){
    return 0;
  }

  buff = photo = (char *) malloc( (size_photo * sizeof(char)) );
  if(photo == NULL){
    perror("Error in the allocation of the memory - photo's string");
    return -1;
  }
  rem_bytes = size_photo * sizeof(char);

  /* Recive the photo */
    while (rem_bytes > 0){
      nbytes_r = read(peer_socket, (char *) buff, rem_bytes * sizeof(char) );
      if(nbytes_r < 0) {
        perror("Error in the communication peer->client (read_client - recive the photo) - GET_NAME_STM - socket (stream)");
        return -1;
      }
      rem_bytes = rem_bytes - nbytes_r;
      buff = buff + nbytes_r;
    }


  fp = fopen(file_name, "wb");
  if(fp == NULL){
    perror("Error in open the file : invalid file_name");
    return -1;
  }

  /* writing of the file */
  fwrite(photo, size_photo, 1 , fp);

  fclose(fp);

  return 1;

}




/*=============================================================================
* long int read_file(char *file_name, char **str)
*
* Arguments:
*             - *file_name   - name of the file that will contain the image
*             - **str        - pointer to the string to store the photo
*
* Return Values:
*             - return size (of the file) if the file was read successfully
*     and stores the information about the photo in *str
*             - return 0 in case of error or if the file_name doesn't exist
*
*
*=============================================================================*/

long int read_file(char *file_name, char **str)
{

  FILE *fp;
  int err;
  long int size;


  /* Open the file */
  fp = fopen(file_name, "rb");
  if(fp == NULL){
    perror("Error in open the file : invalid file_name");
    return 0;
  }

  /* Determination of the file's size */
  err = fseek(fp, 0, SEEK_END);
  if ( err < 0 ){
    perror("fseek");
    return 0;;
  }

  size = ftell(fp);
  if ( size < 0 ){
    perror("ftell");
    return 0;
  }

  rewind(fp);

  *str = (char *)malloc( (size * sizeof(char)) );

  /* reading of the file */
  fread( (void*) (*str), size, 1 , fp);
  /*  if( ferror(fp) ){
    perror("ferror");
    return 0;
  }*/

  fclose(fp);

  return size;
}
