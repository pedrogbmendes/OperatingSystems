#ifndef LIST_H
#define LIST_H

#include<stdio.h>
#include<stdlib.h>

typedef struct _list_node list_node;

typedef struct _list {
  list_node *head;
} list;


void init_list( list *l );
void insert_list( list *l, void *this );
void destroy_list( list *l , void (free_fnt)(void*) );
void delete_node( list *l , list_node *del , void (free_fnt)(void*) );
void* remove_node( list *l , list_node *del );
list_node* inplace_delete ( list *l , list_node *del , list_node *prev , void (free_fnt)(void*) );
list_node* get_head( list *l );
list_node* next_list( list_node *node );
void* get_content( list_node *node );
list_node* cycle( list *l, list_node *node );
int empty_list( list *l );
int contains_list( list *l , void *this , int (cmp_fnt)(void*, void*) );

#endif
