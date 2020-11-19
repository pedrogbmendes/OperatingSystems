#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "photo_data.h"
#include "list.h"

photo_data* new_photo_data( uint32_t id ) {
  photo_data *new = NULL;

  new = (photo_data*) malloc(sizeof(photo_data));

  if ( new == NULL ) {
    return NULL;
  }

  new->id = id;
  new->file_name = NULL;
//  new->file_tag = NULL;
  init_list( &(new->keywords) );

  return new;
}

void free_photo_data( void *this ) {
  photo_data *del = (photo_data*) this;

  if ( del == NULL ) {
    return;
  }

  free(del->file_name);

  int err =remove(del->file_tag);
  if(err == -1){
    perror("file_tag doesn't exist");

  }

  destroy_list( &(del->keywords), free );
}
