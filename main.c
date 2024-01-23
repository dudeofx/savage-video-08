#include <stdio.h>
#include <string.h>

#include "tXMLNode.h"


void main() {  
   tXMLNode *xml_root = tXMLNode_NewFromFile("simple.xml");
   tXMLNode_PrintNode(xml_root, 0);
   tObject_Delete(xml_root);
}
