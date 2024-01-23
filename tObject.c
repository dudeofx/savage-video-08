#include <assert.h>
#include <stdlib.h>

#include "tObject.h"

int next_object_uid = 0x1000;
//-----------------------------------------------------------------------------------------
void tObject_OnDestroy(void *self) {};
//-----------------------------------------------------------------------------------------
tObject *tObject_New() {
   tObject *self;

   self = (tObject *) malloc(sizeof(tObject));
   assert(self != NULL);
   tObject_Init(self);   

   return self;
}
//-----------------------------------------------------------------------------------------
void tObject_Init(tObject *self) {
   self->type = _TYPE_tObject_;
   self->cast = _TYPE_tObject_;
   self->obj_uid = next_object_uid++;
   self->mem_size = sizeof(tObject);  
   self->sanity_check = self->mem_size * (self->type+1) * self->obj_uid; 
   self->func_OnDestroy = tObject_OnDestroy;
}
//-----------------------------------------------------------------------------------------
void tObject_Delete(void *tmp) {
   assert(tmp != NULL);
   tObject *obj = tmp;

   int sanity_check = obj->sanity_check;
   assert(sanity_check == obj->mem_size * (obj->type+1) * obj->obj_uid); 
   obj->func_OnDestroy(obj);
   obj->sanity_check = 0;
   free(obj);
}
//-----------------------------------------------------------------------------------------

