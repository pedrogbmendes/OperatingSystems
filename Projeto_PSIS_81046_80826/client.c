
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>

#include "photo_gallery.h"

void alarm_handler(int sig);

int peer_socket;

int main(int argc, char const *argv[]) {

  int option;
  char gw_add[20], gw_port_str[20];
  in_port_t gw_port;
  uint32_t id, id_photo;
  char file_name1[100], keyword[100];
  char file_name[100], keyword1[100];
  int err;
  char str[20];
  int i=0, aux=0;

  struct sigaction act;

  /* Configures the process termination interruption. */
  act.sa_handler = alarm_handler;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = SA_RESTART;

  sigaction(SIGINT, &act, NULL);


  uint32_t *id_photos;
  char *photo_name;

  FILE *f_net;
  f_net = fopen("network_client.txt", "r");

  /* Gets the adress of the gateway and the port. */
  fgets(gw_add, 20, f_net);
  fgets(gw_port_str, 20, f_net);
  sscanf(gw_port_str, "%d", (int*) &gw_port);

  fclose(f_net);

  peer_socket = gallery_connect(gw_add, gw_port);
  if(peer_socket == -1){
    perror("Error to connect to gateway");
    return -1;
  }else if(peer_socket == 0){
    printf("no peer available\n");
    return 0;
  }else if(peer_socket > 0){
    printf("Gallery connect\n");
  }

  while (1) {

    printf("Select some option\n:"
          "\t 1 : add photo\n"
          "\t 2 : add keyword\n"
          "\t 3 : search photo\n"
          "\t 4 : delete photo\n"
          "\t 5 : get photo name\n"
          "\t 6 : get photo\n");

    fgets(str, 20, stdin);
    sscanf(str, "%d", (int*) &option);

    if((option < 0) || (option > 6) ){

            printf("Choose again\n");
    }else{

      switch (option) {

        case 1:

          printf("Enter the file_name\n");
          fgets(file_name1, sizeof(file_name1), stdin);
          sscanf(file_name1, "%s", file_name);


          if( access( file_name, F_OK ) != -1 ) {
              // file exists
              id = gallery_add_photo(peer_socket, file_name);

              if( id == 0 ){
                printf("Error: Invalid file_name or problems in communication with the server\n");
              }else if(id > 0){
                printf("Photo added: %s : %x\n", file_name1, id );
              }

          } else {
              // file doesn't exist
              printf("File doesn't exist\n");
              break;
          }

        break;

        case 2:

          printf("Enter the keyword to add\n");
          fgets(keyword1, sizeof(keyword1), stdin);
          sscanf(keyword1, "%s", keyword);


          printf("Enter the id of photo\n");
          fgets(str, 20, stdin);
          sscanf(str, "%x", (int*) &aux);

          err = gallery_add_keyword(peer_socket, aux, keyword);

          if ( err == -1){
            printf("Error in communication\n");
          } else if( err == 0 ){
            printf("No photo with this id\n");
          }else if (err ==1 ){
            printf("Keyword added : %s\n", keyword);
          }

        break;

        case 3:
          printf("Enter the keyword to search\n");
          fgets(keyword1, sizeof(keyword1), stdin);
          sscanf(keyword1, "%s", keyword);

          err = gallery_search_photo(peer_socket, keyword, &id_photos);

          if( err == -1 ){
            printf("Error: invalid arguments or network problem\n" );
          }else if( err == 0 ){
            printf("No photo contains the provided keyword\n");
          }else if( err > 0 ){
            for(int j=0; j<err; j++){
              printf("Photo with id %x\n", (int) id_photos[j] );
            }
          }
        break;

        case 4:

          printf("Enter the id of photo to delete\n");
          fgets(str, 20, stdin);
          sscanf(str, "%x", (int*) &aux);

          err = gallery_delete_photo(peer_socket, aux);
          if( err == 1 ){
            printf("Photo deleted\n");
          } else if( err == 0 ){
            printf("No photo with this id\n");
          } else if(err == -1 ){
            printf("ERROR to delete de photo\n");
          }

        break;

        case 5:

        printf("Enter the id of photo to get name\n");
        fgets(str, 20, stdin);
        sscanf(str, "%x", (int*) &aux);

          err = gallery_get_photo_name(peer_socket, aux, &photo_name);
          if( err == 1 ){
            printf("photo_name :%s\n", photo_name);
          }else if( err == 0 ){
            printf("No photo with this id\n");
          }else if( err == -1 ){
            printf("ERROR to get the photo's name\n");
          }
        break;

        case 6:
          printf("Enter the file_name_exit\n");
          fgets(file_name1, sizeof(file_name1), stdin);
          sscanf(file_name1, "%s", file_name);
          printf("Enter the id of photo to donwload\n");
          fgets(str, 20, stdin);
          sscanf(str, "%x", (int*) &aux);



          err = gallery_get_photo(peer_socket, aux, file_name);
          if( err == 1 ){
            printf("Photo donwload\n");
          }else if( err == 0 ){
            printf("No photo with this id\n");
          }else if( err == -1 ){
            printf("ERROR in donwload the photo\n");
          }
        break;

        default:
          return 0;
      }
    }
  }

  return 0;
}



void alarm_handler(int sig){

  close(peer_socket);
  exit(1);
}
