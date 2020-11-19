#include "list.h"

struct _list_node {
  void* this;
  struct _list_node * next;
};


void init_list( list *l )
{
  l->head = NULL;
}


void insert_list(list *l, void *this)
{
  list_node *new;

  /* Memory allocation */
  new = (list_node *) malloc(sizeof(list_node));

  /* Check memory allocation errors */
  if(new == NULL)
    return;

  /* Initialize new node */
  new->this = this;
  new->next = l->head;

  /* Insert. */
  l->head = new;

  return;
}


void destroy_list( list *l , void (free_fnt)(void*) )
{
  list_node * next = NULL;
  list_node * aux = NULL;

  /* Cycle from the first to the last element. */
  for( aux = l->head; aux != NULL; aux = next )
  {
    /* Keep trace of the next node. */
    next = aux->next;

    /* Free current item. */
    free_fnt(aux->this);

    /* Free current node. */
    free(aux);
  }

  return;
}

 void delete_node( list *l , list_node *del , void (free_fnt)(void*) )
 {
   list_node *aux = l->head;

   if(del == l->head){
     /* If the node to delete is the head. */

     l->head = del->next;
     free(del->this);
     free(del);
   }else{
     /* Other nodes. */

     while(del != aux->next){
       aux = aux->next;
       if(aux->next == NULL){
         return;
       }
     }

     aux->next = del->next;
     free_fnt(del->this);
     free(del);
   }
}

void* remove_node( list *l , list_node *del )
{
  list_node *aux = l->head;
  void* rmv = NULL;

  if(del == l->head){
    /* If the node to delete is the head. */

    l->head = del->next;

    rmv = del->this;
    free(del);
  }else{
    /* Other nodes. */

    while(del != aux->next){
      aux = aux->next;
      if(aux->next == NULL){
        return NULL;
      }
    }

    aux->next = del->next;

    rmv = del->this;
    free(del);
  }

  return rmv;
}

list_node *inplace_delete ( list *l , list_node *del , list_node *prev , void (free_fnt)(void*) )
{
  list_node *res;

  if(prev == NULL){
    /* If the node to delete is the head. */

    res = l->head = del->next;
    free(del->this);
    free(del);
  }else{
    /* Other nodes. */

    res = prev->next = del->next;
    free_fnt(del->this);
    free(del);
  }

  return res;
}

list_node *get_head(list *l)
{
  return ((l->head == NULL) ? NULL : l->head);
}


list_node *next_list(list_node *node)
{
  return ((node == NULL) ? NULL : node->next);
}


void *get_content(list_node *node)
{
  return ((node == NULL) ? NULL : node->this);
}


list_node *cycle(list *l, list_node *node)
{
  if(next_list(node) == NULL){
    return get_head(l);
  }else{
    return next_list(node);
  }

}

int empty_list(list *l)
{
  /*if a list is empty return 0 other case return 1*/
  return ((l->head == NULL) ? 0 : 1);
}

int contains_list(list *l, void *this, int (cmp_fnt)(void*, void*) )
{
  list_node * aux = NULL;

  /* Cycle from the first to the last element. */
  for( aux = l->head ; aux != NULL ; aux = aux->next )
  {
    /* Compare element with the reference (this). */
    if ( cmp_fnt(this, aux->this) == 1 ) {
      return 1;
    }
  }

  return 0;
}
