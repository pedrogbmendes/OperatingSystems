#ifndef PHOTO_DATA_H
#define PHOTO_DATA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "list.h"

typedef struct _photo_data {
  uint32_t id;
  char *file_name;
  char file_tag[20];
  long int size_photo;
  list keywords;
} photo_data;

photo_data* new_photo_data( uint32_t id );
void free_photo_data( void *this );

#endif
