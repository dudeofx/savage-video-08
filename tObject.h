#ifndef __tObject_h__
#define __tObject_h__

#include "tTypes.h"
//-----------------------------------------------------------------------------------------
typedef struct _tObject tObject;

struct _tObject {
   unsigned int type;
   unsigned int cast;
   int obj_uid;
   int mem_size;
   int sanity_check;
   
   void (*func_OnDestroy)(void *);
};

//-----------------------------------------------------------------------------------------
tObject *tObject_New();
void tObject_Init(tObject *self);
void tObject_Delete(void *);

void tObject_OnDestroy(void *);
//-----------------------------------------------------------------------------------------
#endif
