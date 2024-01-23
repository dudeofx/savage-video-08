#ifndef __tXMLNode_h__
#define __tXMLNode_h__

//-----------------------------------------------------------------------------------------

typedef struct _tXMLNode tXMLNode;
typedef struct _tXMLAttr tXMLAttr;

#include "tObject.h"

struct _tXMLNode {
   tObject      _;
   unsigned int cast;
   int          node_type;
   char        *tag;
   int          tag_len;
   void        *owner;
   tXMLNode    *parent;
   tXMLNode    *eldest;
   tXMLNode    *sibling;
   tXMLAttr    *attr_list; 
   char        *text;
   int          text_len;
};

struct _tXMLAttr {
   char     *key;
   int       key_len;
   char     *value;
   int       value_len;
   tXMLAttr *next;
};

enum {
   tXML_UNINITIALIZED,
   tXML_NODE,
   tXML_TEXT,
   tXML_INVALID
};

//-----------------------------------------------------------------------------------------
tXMLNode *tXMLNode_New();
tXMLNode *tXMLNode_NewFromFile(const char *fname);

void tXMLNode_Init(tXMLNode *self);
void tXMLNode_OnDestroy(void *);

void tXMLNode_AttachChild(tXMLNode *self, tXMLNode *node);
void tXMLNode_PrintNode(tXMLNode *self, int tab);
void tXMLNode_PrintAttr(tXMLNode *self, int tab);

//-----------------------------------------------------------------------------------------

#endif

