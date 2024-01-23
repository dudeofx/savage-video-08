#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

enum {
   _opt_VALIDATE,
   _opt_CONSTRUCT,
   _opt_INVALID
};

static const unsigned char _byte_count[] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x00 .. 0x0F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x10 .. 0x1F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x20 .. 0x2F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x30 .. 0x3F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x40 .. 0x4F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x50 .. 0x5F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x60 .. 0x6F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x70 .. 0x7F

   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80 .. 0x8F // error
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90 .. 0x9F // error
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xA0 .. 0xAF // error
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xB0 .. 0xBF // error
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xC0 .. 0xCF // 
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xD0 .. 0xDF // 
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 0xE0 .. 0xEF // 
   4, 4, 4, 4, 4, 4, 4, 4,                          // 0xF0 .. 0xF7 // 
   5, 5, 5, 5, 6, 6, 0, 0,                          // 0xF7 .. 0xFF // 
};

static const unsigned int _lower_limit[] = {
   0xFFFFFFFF, // invalid should never happen
   0x00000000,
   0x00000080,
   0x00000800,
   0x00010000,
   0x00200000,
   0x04000000,
};

static const unsigned char _mask[] = {
   0x00, // invalid should never happen
   0x7F,
   0x1F,
   0x0F,
   0x07,
   0x03,
   0x01,
};

#include "tXMLNode.h"

typedef struct {
   int inx;
   int len;
} tInxLen;

typedef struct {
   char     *buff;
   tInxLen   pos;
   int       mode;
   int       reach;
   tXMLNode *root;
} tParseCtrl;

typedef int (*tParsingFunction)(tParseCtrl *);


//-----------------------------------------------------------------------------------------
//==[declarations for private functions]===================================================

//--[utility functions]--------------------------------------------------------------------
static int PrintSyntaxError(tParseCtrl *ctrl);
static int LoadFile(char **data, int *data_size, const char *fname);
static int UTF8_Encode(int code_point, char *dst);
static int UTF8_Decode(char *buff, int buff_lim, int *code_point);
static int RemRefs(char **dst, int *dst_len, char *src, int src_len);

//--[parsing functions]--------------------------------------------------------------------
static int Parse_AttributeSet(tParseCtrl *ctrl, tXMLAttr **attr_list);
static int Parse_AttValue(tParseCtrl *ctrl);
static int Parse_CharData(tParseCtrl *ctrl, tXMLNode *parent);
static int Parse_CharRef(tParseCtrl *ctrl, int *codept);
static int Parse_Comment(tParseCtrl *ctrl);
static int Parse_Content(tParseCtrl *ctrl, tXMLNode *parent);
static int Parse_DecimalDigit(tParseCtrl *ctrl, int *digit);
static int Parse_Digit(tParseCtrl *ctrl);
static int Parse_Document(tParseCtrl *ctrl);
static int Parse_DocTypeDecl(tParseCtrl *ctrl);
static int Parse_Element(tParseCtrl *ctrl, tXMLNode *parent);
static int Parse_EmptyElementTag(tParseCtrl *ctrl, tXMLNode **node);
static int Parse_EndTag(tParseCtrl *ctrl, char *name, int name_len);
static int Parse_EntityRef(tParseCtrl *ctrl, int *codept);
static int Parse_Eq(tParseCtrl *ctrl);
static int Parse_HexadecimalDigit(tParseCtrl *ctrl, int *digit);
static int Parse_Letter(tParseCtrl *ctrl);
static int Parse_MatchPattern(tParseCtrl *ctrl, char *match, int match_lim);
static int Parse_Misc(tParseCtrl *ctrl);
static int Parse_Name(tParseCtrl *ctrl);
static int Parse_NameChar(tParseCtrl *ctrl);
static int Parse_Prolog(tParseCtrl *ctrl);
static int Parse_Reference(tParseCtrl *ctrl, int *codept);
static int Parse_StartTag(tParseCtrl *ctrl, char **name, int *name_len, tXMLNode **node);
static int Parse_WhiteSpace(tParseCtrl *ctrl);
static int Parse_XMLDecl(tParseCtrl *ctrl);

//-----------------------------------------------------------------------------------------
//==[definitions for private functions]====================================================
//-----------------------------------------------------------------------------------------
static int PrintSyntaxError(tParseCtrl *ctrl) {
   int line = 1;
   int mark = 0;
      
   for (int i = 0; i < ctrl->reach; i++) {
      if (ctrl->buff[i] == '\n') {
         line++;
         mark = i+1;
      }
   }

   printf("syntax error:\n   Line:%i near:", line);

   int inx;
   int lim;

   if (ctrl->reach - mark > 32) { 
      inx = ctrl->reach - 32;
   } else {
      inx = mark;
   }
   while ((ctrl->buff[inx] & 0xC0) == 0x80) inx++;
      
   lim = ctrl->reach;
   for (int i = 0; i < 32; i++) {
      if (lim == ctrl->pos.len) break;
      if (ctrl->buff[ctrl->reach+i] == '\n') {
         lim = ctrl->reach+i;
         break;
      }
      if ((ctrl->buff[ctrl->reach+i] & 0xC0) != 0x80) lim = ctrl->reach+i;
   }
   printf("\"%.*s\"\n", lim - inx, ctrl->buff + inx);
}
//-----------------------------------------------------------------------------------------
static int LoadFile(char **data, int *data_size, const char *fname) {  
   FILE *f;
   int fsize;

   assert(fname != NULL);
   assert(data != NULL);
   assert(data_size != NULL);
   
   f = fopen(fname, "rb");
   assert(f != NULL);
   
   fseek(f, 0, SEEK_END);
   fsize = ftell(f);
   assert(fsize != 0);
   
   *data = (char *) malloc(fsize);
   *data_size = fsize;
   
   fseek(f, 0, SEEK_SET);
   fread(*data, 1, fsize, f);
   fclose(f);
   
   return fsize;
}
//-----------------------------------------------------------------------------------------
static int UTF8_Decode(char *buff, int buff_lim, int *code_point) {
   assert(code_point != NULL);
   *code_point = 0;

   if (buff_lim == 0) return 0; // error: buffer overflow

   int byte_count = 0; 
   int byte_size = 0;
   
   byte_size = byte_count = _byte_count[(unsigned char) *buff];
   
   if (byte_count == 0) return 0; // error: orphaned continuation byte 

   if (buff_lim < byte_size) return 0; // error: buffer truncated

   *code_point = (*(buff++) & _mask[byte_size]);
   byte_count--;
   if (byte_count == 0) return byte_size;
   buff_lim--;
   while (buff_lim > 0) {
      if ((0xC0 & *buff) != 0x80) return 0; // error: expected a continuation byte
      *code_point = ((*code_point << 6) | (0x3F & *buff)) ;
      byte_count--;
      if (byte_count == 0) {
         if (*code_point < _lower_limit[byte_size]) return 0; // overlong check just curtisy
         return byte_size;
      }
      buff++;
      buff_lim--;
   };
   
   return 0; // if it gets here it means data is truncated
}
//-----------------------------------------------------------------------------------------
// dst is a buffer where uft8 sequence is stored
// dst buffer must be at least 5 bytes long. No checking is done. a null byte is appended at the end of sequence.
// function returns the number of bytes of utf8 sequence excluding the null.
//
// set dst = NULL to simply get the bytes the encoded sequence would take up;
static int UTF8_Encode(int code_point, char *dst) {

  if (code_point < 0) {
     if (dst != NULL) dst[0] = '\0';
     return 0;
  }
  
  if (code_point < 0x80) {
      if (dst != NULL) {
         dst[0] = code_point;
         dst[1] = '\0';
      }
      return 1;
   } 
   if (code_point < 0x0800) {
      if (dst != NULL) {
         dst[0] = ((code_point >> 6)  & 0x1F) | 0xC0;
         dst[1] = (code_point & 0x3F) | 0x80;
         dst[2] = '\0';
      }
      return 2;
   }
   if (code_point < 0x010000) {
      if (dst != NULL) {
         dst[0] = ((code_point >> 12) & 0x0F) | 0xE0;
         dst[1] = ((code_point >> 6)  & 0x3F) | 0x80;
         dst[2] = (code_point & 0x3F) | 0x80;
         dst[3] = '\0';
      }
      return 3;
   }
   if (code_point < 0x110000) {
      if (dst != NULL) {
         dst[0] = ((code_point >> 18) & 0x07) | 0xF0;
         dst[1] = ((code_point >> 12) & 0x3F) | 0x80;
         dst[2] = ((code_point >> 6)  & 0x3F) | 0x80;
         dst[3] = (code_point & 0x3F) | 0x80;
         dst[4] = '\0';
      }
      return 4;
   }

   if (dst != NULL) dst[0] = '\0';
   return 0;
}
//-----------------------------------------------------------------------------------------
// function assumes a well-formed XML string (no "<", ">", "&", or invalid codepoints)
// creates a new string "dst" with all refrences from XML string "src" decoded
static int RemRefs(char **dst, int *dst_len, char *src, int src_len) {
   int ofs;
   int parse;
   int codept;
   int byte_count;
   int neta;

   tParseCtrl ctrl = {
      .buff = src,
      .pos.inx = 0,
      .pos.len = src_len,
      .mode = _opt_VALIDATE,
      .reach = 0,
      .root = NULL,
   };

   // count bytes that new string will take
   ofs = 0;
   neta = 0;
   while (ofs < src_len) {
      byte_count = UTF8_Decode(src+ofs, src_len-ofs, &codept);   
      if (byte_count <= 0) return 0;  // should never happen on a valid "src" string
      if (codept == '&') {
         ctrl.pos.inx = ofs;
         byte_count = Parse_Reference(&ctrl, &codept);
         neta += UTF8_Encode(codept, NULL);
      } else {
         neta += byte_count;
      }
      ofs += byte_count;
   }
   *dst = (char *) malloc(neta+1);
   *dst_len = neta;
   
   // render new string;
   ofs = 0;
   neta = 0;
   while (ofs < src_len) {
      byte_count = UTF8_Decode(src+ofs, src_len-ofs, &codept);   
      if (byte_count <= 0) return 0;  // should never happen on a valid "src" string
      if (codept == '&') {
         ctrl.pos.inx = ofs;
         byte_count = Parse_Reference(&ctrl, &codept);
         neta += UTF8_Encode(codept, *dst+neta);
      } else {
         memcpy(*dst+neta, src+ofs, byte_count);
         neta += byte_count;
      }
      ofs += byte_count;
   }
   (*dst)[neta] = '\0';
   
   return 0;
}
//--[parsing functions]--------------------------------------------------------------------
static int Parse_AttributeSet(tParseCtrl *ctrl, tXMLAttr **attr_list) {
   int parse;
   int neta = 0;
   int tmp;
   tXMLAttr *head = NULL;
   tXMLAttr *tail = NULL;
   tXMLAttr *attr = NULL;
   char *key;
   int   key_len;
   char *value;
   int   value_len;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->mode == _opt_CONSTRUCT) *attr_list = head;
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   do {
      tmp = 0;

      // required
      parse = Parse_WhiteSpace(ctrl);
      if (parse <= 0) return CleanExit(neta);
      ctrl->pos.inx += parse;
      tmp += parse;

      // required
      parse = Parse_Name(ctrl);
      if (parse <= 0) return CleanExit(neta);
      key = ctrl->buff + ctrl->pos.inx;
      key_len = parse;
      ctrl->pos.inx += parse;
      tmp += parse;

      // required
      parse = Parse_Eq(ctrl);
      if (parse <= 0) return CleanExit(neta);
      ctrl->pos.inx += parse;
      tmp += parse;

      // required
      parse = Parse_AttValue(ctrl);
      if (parse <= 0) return CleanExit(neta);
      value = ctrl->buff + ctrl->pos.inx;
      value_len = parse;
      ctrl->pos.inx += parse;
      tmp += parse;

      neta += tmp;
      
      if (ctrl->mode == _opt_CONSTRUCT) {
         attr = (tXMLAttr *) malloc(sizeof(tXMLAttr));

         RemRefs(&(attr->key), &(attr->key_len), key, key_len);

         // note the +1 and -2 is a cheap trick to remove the quotes. Should always work.
         RemRefs(&(attr->value), &(attr->value_len), value+1, value_len-2);
         
         attr->next = NULL;
         
         if (head == NULL) {
            head = tail = attr;
         } else {
            tail->next = attr;
            tail = tail->next;
         }
      }
   } while (1);

   return CleanExit(0); // impossible to get here
}
//-----------------------------------------------------------------------------------------
static int Parse_AttValue(tParseCtrl *ctrl) {  
   assert(ctrl != NULL);

   int parse;
   int codept;
   int neta = 0;
   int quote;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................
   // get leading quote
   parse = UTF8_Decode(ctrl->buff + ctrl->pos.inx, ctrl->pos.len - ctrl->pos.inx, &codept);   
   if (parse <= 0) return CleanExit(0);
   if ((codept != '"') && (codept != '\'')) return CleanExit(0);
   quote = codept;
   ctrl->pos.inx += parse;
   neta += parse;
   
   // get content;
   do {
      parse = UTF8_Decode(ctrl->buff + ctrl->pos.inx, ctrl->pos.len - ctrl->pos.inx, &codept);   
      if (parse > 0) {
         if (codept == quote) {
            return CleanExit(neta + parse);
         }
         if (codept == '<') return CleanExit(0);
         if (codept == '&') {
            parse = Parse_Reference(ctrl, NULL);
            if (parse <= 0) return CleanExit(0);
         }
         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);
   
   return CleanExit(0); // data was truncated?
}
//-----------------------------------------------------------------------------------------
static int Parse_Comment(tParseCtrl *ctrl) {
   int parse;
   int neta = 0;
   int codept;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   // required entry marker "<!--"
   parse = Parse_MatchPattern(ctrl, "<!--", 4);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
   
   do {
      parse = UTF8_Decode(ctrl->buff + ctrl->pos.inx, ctrl->pos.len - ctrl->pos.inx, &codept);   
      if (parse > 0) {
         if (codept == '-') if (Parse_MatchPattern(ctrl, "--", 2)) break;

         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);

   // required exit marker "-->"
   parse = Parse_MatchPattern(ctrl, "-->", 3);
   if (parse <= 0) return CleanExit(0);

   ctrl->pos.inx += parse;
   neta += parse;

   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_Content(tParseCtrl *ctrl, tXMLNode *parent) {
   int parse;
   int neta = 0;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   while (1) {
      // optional
      parse = Parse_Element(ctrl, parent);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
         continue;
      }

      // optional
      parse = Parse_CharData(ctrl, parent);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
         continue;
      }

      // optional
      parse = Parse_Reference(ctrl, NULL);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
         continue;
      }

      // not supporting CDSect 

      // not supporting Processing Instructions

      parse = Parse_Comment(ctrl);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
         continue;
      }

      break;
   }
   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_CharData(tParseCtrl *ctrl, tXMLNode *parent) {
   int neta = 0;
   int parse;
   int codept;

   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if ((ctrl->mode == _opt_CONSTRUCT) && (code > 0)) {
         tXMLNode *node = tXMLNode_New();
         node->node_type = tXML_TEXT;
         RemRefs(&(node->text), &(node->text_len), ctrl->buff+mark.inx, code);
         tXMLNode_AttachChild(parent, node);
      }
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   do {
      parse = UTF8_Decode(ctrl->buff + ctrl->pos.inx, ctrl->pos.len - ctrl->pos.inx, &codept);   
      if (parse > 0) {
         if (codept == '<') return CleanExit(neta); // not supporting markup hence this character marks end-of-pattern.
         if (codept == '&') {
            parse = Parse_Reference(ctrl, NULL);
            if (parse <= 0) return CleanExit(0); // if its not a reference then its not a legal character
         }

         if (Parse_MatchPattern(ctrl, "]]>", 3)) return CleanExit(0);   // Not supporting CDATA section hence its a fail for now

         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);
   
   return CleanExit(0);
}
//-----------------------------------------------------------------------------------------
static int Parse_CharRef(tParseCtrl *ctrl, int *codept) {
//  	'&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
   int parse;
   int neta = 0;
   int hexcode = 0;
   int value;
   int digit;
   int multiplier =  10;

   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................
   int CheckValidChar(int value) {
      if (value == 0x9) return 1;
      if (value == 0xa) return 1;
      if (value == 0xd) return 1;
      if ((value >= 0x20) && (value <= 0xd7ff)) return 1;
      if ((value >= 0xe000) && (value <= 0xfffd)) return 1;
      if ((value >= 0x10000) && (value <= 0x10ffff)) return 1;
      return 0;
   }
   //......................................................................................

   // required
   parse = Parse_MatchPattern(ctrl, "&#", 2);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // modifier
   parse = Parse_MatchPattern(ctrl, "x", 1);
   if (parse > 0) {
      multiplier = 16;
      hexcode = 1;
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // required initial digit
   if (hexcode) {
      parse = Parse_HexadecimalDigit(ctrl, &value);
   } else {
      parse = Parse_DecimalDigit(ctrl, &value);
   }

   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;  
   neta += parse;
   
   // optional digits
   do {
      if (hexcode) {
         parse = Parse_HexadecimalDigit(ctrl, &digit);
      } else {
         parse = Parse_DecimalDigit(ctrl, &digit);
      }

      if (parse > 0) {
         value = value * multiplier + digit;
         ctrl->pos.inx += parse;  
         neta += parse;
         if (value > 0x10FFFF) return CleanExit(0); // exceeded range
      }
   } while (parse > 0);

   // required ';'
   parse = Parse_MatchPattern(ctrl, ";", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
   
   if (CheckValidChar(value) == 0) return CleanExit(0);

   if (codept != NULL) *codept = value;

   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_DecimalDigit(tParseCtrl *ctrl, int *digit) {
   int codept;
   int parse;
   
   parse = UTF8_Decode(ctrl->buff+ctrl->pos.inx, ctrl->pos.len-ctrl->pos.inx, &codept);
   if (parse <= 0) return 0;
   if ((codept >= '0') && (codept <= '9')) {
      if (digit != NULL) *digit = codept - '0';
      return parse;
   }
   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_Digit(tParseCtrl *ctrl) {
   int seq[15][2] = {
      {0x0030,0x0039}, {0x0660,0x0669}, {0x06F0,0x06F9}, {0x0966,0x096F}, {0x09E6,0x09EF}, 
      {0x0A66,0x0A6F}, {0x0AE6,0x0AEF}, {0x0B66,0x0B6F}, {0x0BE7,0x0BEF}, {0x0C66,0x0C6F}, 
      {0x0CE6,0x0CEF}, {0x0D66,0x0D6F}, {0x0E50,0x0E59}, {0x0ED0,0x0ED9}, {0x0F20,0x0F29}
   };
   int codept = 0;
   int parse;
   
   parse = UTF8_Decode(ctrl->buff+ctrl->pos.inx, ctrl->pos.len-ctrl->pos.inx, &codept);
   if (parse <= 0) return 0; // not a valid utf8 code point

   for (int i = 0; i < 15; i++) {
      if ((seq[i][0] <= codept) && (seq[i][1] >= codept)) {
         return parse;
      }
   }
   
   return 0; // not a valid letter
}
//-----------------------------------------------------------------------------------------
static int Parse_Document(tParseCtrl *ctrl) {
   int parse;
   int neta = 0;
  
   assert(ctrl != NULL);
   assert(ctrl->pos.len > 0);
   assert(ctrl->mode >= 0);
   assert(ctrl->mode < _opt_INVALID);

   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................
   
   // optional
   parse = Parse_Prolog(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // Required
   parse = Parse_Element(ctrl, NULL);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
   
   // zero or more
   do {
      parse = Parse_Misc(ctrl);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);

   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_DocTypeDecl(tParseCtrl *ctrl) {
   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_Element(tParseCtrl *ctrl, tXMLNode *parent) {
   int       parse;
   int       neta = 0;
   char     *tag;
   int       tag_len;
   tXMLNode *node = NULL;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if ((ctrl->mode == _opt_CONSTRUCT) && (node != NULL)) {
         if (parent == NULL) {
            ctrl->root = node;
         } else {
            tXMLNode_AttachChild(parent, node);
         }
      }
      // keep tract off furthest reach in case of error
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      // restore previous position
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................
   
   // optional
   parse = Parse_EmptyElementTag(ctrl, &node);
   if (parse > 0) return CleanExit(parse);

   // optional
   parse = Parse_StartTag(ctrl, &tag, &tag_len, &node);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
      
      parse = Parse_Content(ctrl, node);
      if (parse >= 0) {
         ctrl->pos.inx += parse;
         neta += parse;
      }
      
      parse = Parse_EndTag(ctrl, tag, tag_len);
      if (parse > 0) {
         neta += parse;
         return CleanExit(neta);
      }
   }

   return CleanExit(0);
}
//-----------------------------------------------------------------------------------------
static int Parse_EmptyElementTag(tParseCtrl *ctrl, tXMLNode **node) {
   int       parse;
   int       neta = 0;
   char     *tag;
   int       tag_len;
   tXMLAttr *attr_list = NULL;
   
   assert(ctrl != NULL); 

   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if ((ctrl->mode == _opt_CONSTRUCT) && (code > 0)) {
         *node = tXMLNode_New();
         (*node)->node_type = tXML_NODE;
         (*node)->tag = (char *) malloc(tag_len+1);
         memcpy((*node)->tag, tag, tag_len);
         (*node)->tag[tag_len] = '\0';
         (*node)->tag_len = tag_len;
         (*node)->attr_list = attr_list;
      }
      // keep tract off furthest reach in case of error
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      // restore previous position
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   char *buff = ctrl->buff+ctrl->pos.inx;
   int lim = ctrl->pos.len-ctrl->pos.inx;

   // required
   parse = Parse_MatchPattern(ctrl, "<", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // required
   parse = Parse_Name(ctrl);
   if (parse <= 0) return CleanExit(0);
   tag = ctrl->buff + ctrl->pos.inx;
   tag_len = parse;
   ctrl->pos.inx += parse;
   neta += parse;

   // optional attributes
   parse = Parse_AttributeSet(ctrl, &attr_list);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // optional
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // required
   parse = Parse_MatchPattern(ctrl, "/>", 2);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
   
   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_EndTag(tParseCtrl *ctrl, char *tag, int tag_len) {
   int parse;
   int neta = 0;

   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   // required
   parse = Parse_MatchPattern(ctrl, "</", 2);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // required match the tag
   parse = Parse_MatchPattern(ctrl, tag, tag_len);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // optional
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // required
   parse = Parse_MatchPattern(ctrl, ">", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   return neta;
}
//-----------------------------------------------------------------------------------------
static int Parse_EntityRef(tParseCtrl *ctrl, int *codept) {
   int parse;
   int neta = 0;
   char *pattern[5] = {"amp", "lt", "gt", "apos", "quot"};
   int length[5] = {3, 2, 2, 4, 4};
   int codes[5] = {'&', '<', '>', '\'', '"'};

   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   // required
   parse = Parse_MatchPattern(ctrl, "&", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // one pattern plus ';' is required
   for (int i = 0; i < 5; i++) {
      parse = Parse_MatchPattern(ctrl, pattern[i], length[i]);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
         parse = Parse_MatchPattern(ctrl, ";", 1);
         if (parse > 0) {
            if (codept != NULL) *codept = codes[i];
            return CleanExit(neta+parse);
         }
         return CleanExit(0);
      }
   }
     
   return CleanExit(0);
}
//-----------------------------------------------------------------------------------------
static int Parse_HexadecimalDigit(tParseCtrl *ctrl, int *digit) {
   int codept;
   int parse;
   
   parse = UTF8_Decode(ctrl->buff+ctrl->pos.inx, ctrl->pos.len-ctrl->pos.inx, &codept);
   if (parse <= 0) return 0;
   if ((codept >= '0') && (codept <= '9')) {
      if (digit != NULL) *digit = codept - '0';
      return parse;
   }

   if ((codept >= 'a') && (codept <= 'f')) {
      if (digit != NULL) *digit = codept - 'a' + 10;
      return parse;
   }
   
   if ((codept >= 'A') && (codept <= 'F')) {
      if (digit != NULL) *digit = codept - 'A' + 10;
      return parse;
   }
   
   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_Eq(tParseCtrl *ctrl) {
   int parse;
   int neta = 0;
   int codept;
   
   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   // optional whitespaces
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // required '='
   parse = UTF8_Decode(ctrl->buff + ctrl->pos.inx, ctrl->pos.len - ctrl->pos.inx, &codept);   
   if (parse <= 0) return CleanExit(0);
   if (codept != '=') return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
     
   // optional whitespaces
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_Letter(tParseCtrl *ctrl) {
   int seq[205][2] = { 
      // BaseChar
      {0x0041,0x005A}, {0x0061,0x007A}, {0x00C0,0x00D6}, {0x00D8,0x00F6}, {0x00F8,0x00FF}, {0x0100,0x0131}, {0x0134,0x013E}, 
      {0x0141,0x0148}, {0x014A,0x017E}, {0x0180,0x01C3}, {0x01CD,0x01F0}, {0x01F4,0x01F5}, {0x01FA,0x0217}, {0x0250,0x02A8}, 
      {0x02BB,0x02C1}, {0x0386,0x0386}, {0x0388,0x038A}, {0x038C,0x038C}, {0x038E,0x03A1}, {0x03A3,0x03CE}, {0x03D0,0x03D6},
      {0x03DA,0x03DA}, {0x03DC,0x03DC}, {0x03DE,0x03DE}, {0x03E0,0x03E0}, {0x03E2,0x03F3}, {0x0401,0x040C}, {0x040E,0x044F}, 
      {0x0451,0x045C}, {0x045E,0x0481}, {0x0490,0x04C4}, {0x04C7,0x04C8}, {0x04CB,0x04CC}, {0x04D0,0x04EB}, {0x04EE,0x04F5}, 
      {0x04F8,0x04F9}, {0x0531,0x0556}, {0x0559,0x0559}, {0x0561,0x0586}, {0x05D0,0x05EA}, {0x05F0,0x05F2}, {0x0621,0x063A}, 
      {0x0641,0x064A}, {0x0671,0x06B7}, {0x06BA,0x06BE}, {0x06C0,0x06CE}, {0x06D0,0x06D3}, {0x06D5,0x06D5}, {0x06E5,0x06E6}, 
      {0x0905,0x0939}, {0x093D,0x093D}, {0x0958,0x0961}, {0x0985,0x098C}, {0x098F,0x0990}, {0x0993,0x09A8}, {0x09AA,0x09B0},
      {0x09B2,0x09B2}, {0x09B6,0x09B9}, {0x09DC,0x09DD}, {0x09DF,0x09E1}, {0x09F0,0x09F1}, {0x0A05,0x0A0A}, {0x0A0F,0x0A10}, 
      {0x0A13,0x0A28}, {0x0A2A,0x0A30}, {0x0A32,0x0A33}, {0x0A35,0x0A36}, {0x0A38,0x0A39}, {0x0A59,0x0A5C}, {0x0A5E,0x0A5E},
      {0x0A72,0x0A74}, {0x0A85,0x0A8B}, {0x0A8D,0x0A8D}, {0x0A8F,0x0A91}, {0x0A93,0x0AA8}, {0x0AAA,0x0AB0}, {0x0AB2,0x0AB3}, 
      {0x0AB5,0x0AB9}, {0x0ABD,0x0ABD}, {0x0AE0,0x0AE0}, {0x0B05,0x0B0C}, {0x0B0F,0x0B10}, {0x0B13,0x0B28}, {0x0B2A,0x0B30}, 
      {0x0B32,0x0B33}, {0x0B36,0x0B39}, {0x0B3D,0x0B3D}, {0x0B5C,0x0B5D}, {0x0B5F,0x0B61}, {0x0B85,0x0B8A}, {0x0B8E,0x0B90}, 
      {0x0B92,0x0B95}, {0x0B99,0x0B9A}, {0x0B9C,0x0B9C}, {0x0B9E,0x0B9F}, {0x0BA3,0x0BA4}, {0x0BA8,0x0BAA}, {0x0BAE,0x0BB5}, 
      {0x0BB7,0x0BB9}, {0x0C05,0x0C0C}, {0x0C0E,0x0C10}, {0x0C12,0x0C28}, {0x0C2A,0x0C33}, {0x0C35,0x0C39}, {0x0C60,0x0C61}, 
      {0x0C85,0x0C8C}, {0x0C8E,0x0C90}, {0x0C92,0x0CA8}, {0x0CAA,0x0CB3}, {0x0CB5,0x0CB9}, {0x0CDE,0x0CDE}, {0x0CE0,0x0CE1}, 
      {0x0D05,0x0D0C}, {0x0D0E,0x0D10}, {0x0D12,0x0D28}, {0x0D2A,0x0D39}, {0x0D60,0x0D61}, {0x0E01,0x0E2E}, {0x0E30,0x0E30},
      {0x0E32,0x0E33}, {0x0E40,0x0E45}, {0x0E81,0x0E82}, {0x0E84,0x0E84}, {0x0E87,0x0E88}, {0x0E8A,0x0E8A}, {0x0E8D,0x0E8D}, 
      {0x0E94,0x0E97}, {0x0E99,0x0E9F}, {0x0EA1,0x0EA3}, {0x0EA5,0x0EA5}, {0x0EA7,0x0EA7}, {0x0EAA,0x0EAB}, {0x0EAD,0x0EAE}, 
      {0x0EB0,0x0EB0}, {0x0EB2,0x0EB3}, {0x0EBD,0x0EBD}, {0x0EC0,0x0EC4}, {0x0F40,0x0F47}, {0x0F49,0x0F69}, {0x10A0,0x10C5}, 
      {0x10D0,0x10F6}, {0x1100,0x1100}, {0x1102,0x1103}, {0x1105,0x1107}, {0x1109,0x1109}, {0x110B,0x110C}, {0x110E,0x1112}, 
      {0x113C,0x113C}, {0x113E,0x113E}, {0x1140,0x1140}, {0x114C,0x114C}, {0x114E,0x114E}, {0x1150,0x1150}, {0x1154,0x1155}, 
      {0x1159,0x1159}, {0x115F,0x1161}, {0x1163,0x1163}, {0x1165,0x1165}, {0x1167,0x1167}, {0x1169,0x1169}, {0x116D,0x116E}, 
      {0x1172,0x1173}, {0x1175,0x1175}, {0x119E,0x119E}, {0x11A8,0x11A8}, {0x11AB,0x11AB}, {0x11AE,0x11AF}, {0x11B7,0x11B8}, 
      {0x11BA,0x11BA}, {0x11BC,0x11C2}, {0x11EB,0x11EB}, {0x11F0,0x11F0}, {0x11F9,0x11F9}, {0x1E00,0x1E9B}, {0x1EA0,0x1EF9}, 
      {0x1F00,0x1F15}, {0x1F18,0x1F1D}, {0x1F20,0x1F45}, {0x1F48,0x1F4D}, {0x1F50,0x1F57}, {0x1F59,0x1F59}, {0x1F5B,0x1F5B}, 
      {0x1F5D,0x1F5D}, {0x1F5F,0x1F7D}, {0x1F80,0x1FB4}, {0x1FB6,0x1FBC}, {0x1FBE,0x1FBE}, {0x1FC2,0x1FC4}, {0x1FC6,0x1FCC}, 
      {0x1FD0,0x1FD3}, {0x1FD6,0x1FDB}, {0x1FE0,0x1FEC}, {0x1FF2,0x1FF4}, {0x1FF6,0x1FFC}, {0x2126,0x2126}, {0x212A,0x212B},
      {0x212E,0x212E}, {0x2180,0x2182}, {0x3041,0x3094}, {0x30A1,0x30FA}, {0x3105,0x312C}, {0xAC00,0xD7A3}, 
      // Idiographic
      {0x4E00,0x9FA5}, {0x3007,0x3007}, {0x3021,0x3029}
   };
   int codept = 0;
   int parse;
   char *buff = ctrl->buff + ctrl->pos.inx;
   int lim = ctrl->pos.len - ctrl->pos.inx;

   parse = UTF8_Decode(buff, lim, &codept);
   if (parse <= 0) return 0; // not a valid utf8 code point
   
   for (int i = 0; i < 205; i++) {
      if ((seq[i][0] <= codept) && (seq[i][1] >= codept)) {
         return parse;
      }
   }
   
   return 0; // not a valid letter
}
//-----------------------------------------------------------------------------------------
static int Parse_MatchPattern(tParseCtrl *ctrl, char *match, int match_lim) {
   assert(ctrl != NULL);
   assert(match != NULL);

   if (ctrl->pos.len - ctrl->pos.inx < match_lim) return 0;    // buffer is too small to hold pattern

   char *buff = ctrl->buff + ctrl->pos.inx;

   for (int i = 0; i < match_lim; i++) {
      if (*buff != *match) return 0;   // not a match
      buff++;
      match++;
   }

   return match_lim;
}
//-----------------------------------------------------------------------------------------
static int Parse_Misc(tParseCtrl *ctrl) {

   assert(ctrl != NULL);

   char *buff = ctrl->buff + ctrl->pos.inx;
   int   lim = ctrl->pos.len - ctrl->pos.inx;
   int   ofs = 0;
   int   parse;

   // optional
   parse = Parse_Comment(ctrl);
   if (parse > 0) return parse;   
   
   // optional
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) return parse;

   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_Name(tParseCtrl *ctrl) {
   int neta = 0;
   int parse;

   assert(ctrl != NULL);
   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   parse = Parse_Letter(ctrl);
   if (parse <= 0) {
      parse = Parse_MatchPattern(ctrl, "_", 1);
      if (parse <= 0) {
         parse = Parse_MatchPattern(ctrl, "-", 1);
         return CleanExit(0);
      }
   }

   ctrl->pos.inx += parse;
   neta += parse;
   do {
      parse = Parse_NameChar(ctrl);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);

   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_NameChar(tParseCtrl *ctrl) {
   int parse;

   parse = Parse_Letter(ctrl);
   if (parse > 0) return parse;

   parse = Parse_Digit(ctrl);
   if (parse > 0) return parse;

   parse = Parse_MatchPattern(ctrl, ".", 1);
   if (parse > 0) return parse;
   
   parse = Parse_MatchPattern(ctrl, "-", 1);
   if (parse > 0) return parse;
   
   parse = Parse_MatchPattern(ctrl, "_", 1);
   if (parse > 0) return parse;
   
   parse = Parse_MatchPattern(ctrl, ":", 1);
   if (parse > 0) return parse;
   
   // not supporting combining chars

   // not supporting extender chars
   
   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_Prolog(tParseCtrl *ctrl) {
   int parse;
   int neta = 0;

   assert(ctrl != NULL);

   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................
   
   // optional
   parse = Parse_XMLDecl(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }
   
   // zero or more
   do {
      parse = Parse_Misc(ctrl);
      if (parse > 0) {
         ctrl->pos.inx += parse;
         neta += parse;
      }
   } while (parse > 0);
   
   // optional
   parse = Parse_DocTypeDecl(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
      // zero or more
      do {
         parse = Parse_Misc(ctrl);
         if (parse > 0) {
            ctrl->pos.inx += parse;
            neta += parse;
         }
      } while (parse > 0);
   } 
   
   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_Reference(tParseCtrl *ctrl, int *codept) {
   int parse;

   parse = Parse_EntityRef(ctrl, codept);
   if (parse > 0) {
      return parse;
   }
   
   parse = Parse_CharRef(ctrl, codept);
   if (parse > 0) {
      return parse;
   }
   
   return 0;
}
//-----------------------------------------------------------------------------------------
static int Parse_StartTag(tParseCtrl *ctrl, char **tag, int *tag_len, tXMLNode **node) {
   int parse;
   int neta = 0;
   tXMLAttr *attr_list = NULL;
   
   assert(ctrl != NULL); 
   assert(tag != NULL);
   assert(tag_len != NULL);

   tInxLen mark = ctrl->pos; // save position to be restored at CleanExit()
   //......................................................................................
   int CleanExit(int code) {
      if ((ctrl->mode == _opt_CONSTRUCT) && (code > 0)) {
         *node = tXMLNode_New();
         (*node)->node_type = tXML_NODE;
         (*node)->tag = (char *) malloc(*tag_len+1);
         memcpy((*node)->tag, *tag, *tag_len);
         (*node)->tag[*tag_len] = '\0';
         (*node)->tag_len = *tag_len;
         (*node)->attr_list = attr_list;
      }

      if (ctrl->pos.inx > ctrl->reach) ctrl->reach = ctrl->pos.inx;
      ctrl->pos = mark;
      return code;
   }
   //......................................................................................

   char *buff = ctrl->buff+ctrl->pos.inx;
   int lim = ctrl->pos.len-ctrl->pos.inx;

   // required
   parse = Parse_MatchPattern(ctrl, "<", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;

   // required
   parse = Parse_Name(ctrl);
   if (parse <= 0) return CleanExit(0);
   *tag = buff + neta;
   *tag_len = parse;
   ctrl->pos.inx += parse;
   neta += parse;

   // optional attributes
   parse = Parse_AttributeSet(ctrl, &attr_list);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // optional
   parse = Parse_WhiteSpace(ctrl);
   if (parse > 0) {
      ctrl->pos.inx += parse;
      neta += parse;
   }

   // required
   parse = Parse_MatchPattern(ctrl, ">", 1);
   if (parse <= 0) return CleanExit(0);
   ctrl->pos.inx += parse;
   neta += parse;
   
   return CleanExit(neta);
}
//-----------------------------------------------------------------------------------------
static int Parse_WhiteSpace(tParseCtrl *ctrl) {
   char *seq = "\x20\x9\xD\xA";  
   int   ofs = 0;
   char *buff = ctrl->buff + ctrl->pos.inx;
   int   lim = ctrl->pos.len - ctrl->pos.inx;
   
   while (ofs < lim) {
      int i;
      for (i = 0; i < 4; i++) {
         if (buff[ofs] == seq[i]) {
            break;
         }
      }
      if (i == 4) break; // didn't match a white space
      ofs++;
   }
  
   return ofs;
}
//-----------------------------------------------------------------------------------------
static int Parse_XMLDecl(tParseCtrl *ctrl) {
   return 0;
}
//-----------------------------------------------------------------------------------------
//==[constructor]==========================================================================
tXMLNode *tXMLNode_New() {
   tXMLNode *self;

   self = (tXMLNode *) malloc(sizeof(tXMLNode));
   assert(self != NULL);
     
   self->cast = _TYPE_tXMLNode_;
   ((tObject *) self)->type = _TYPE_tXMLNode_;
   ((tObject *) self)->mem_size = sizeof(tXMLNode);
   tXMLNode_Init(self);   

   return self;
}
//-----------------------------------------------------------------------------------------
tXMLNode *tXMLNode_NewFromFile(const char *fname) {
   tXMLNode *self = NULL;
   char *data;
   int   data_size;

   LoadFile(&data, &data_size, fname);

   assert(data_size > 0);
    
   tParseCtrl ctrl = {
      .buff = data,
      .pos.inx = 0,
      .pos.len = data_size,
      .mode = _opt_VALIDATE,
      .reach = 0,
      .root = NULL,
   };

   // validate
   Parse_Document(&ctrl);
   if (ctrl.reach < data_size) {
      PrintSyntaxError(&ctrl);
      return NULL;
   }

   // contruct
   ctrl.mode = _opt_CONSTRUCT;   
   Parse_Document(&ctrl);
   
   
   return ctrl.root;
}
//-----------------------------------------------------------------------------------------
//==[destructor]===========================================================================
void tXMLNode_OnDestroy(void *tmp) {
   assert(tmp != NULL);
   tXMLNode *node = (tXMLNode *) tmp;

   // free the children first
   tXMLNode *child = node->eldest;
   while (child != NULL) {
      tXMLNode *marked = child;
      child = child->sibling;
      tObject_Delete(marked);
   }
   
   if (node->tag != NULL) free(node->tag);
   if (node->text != NULL) free(node->text);

   // free attributes
   tXMLAttr *attr = node->attr_list;
   while (attr != NULL) {
      tXMLAttr *marked = attr;
      attr = attr->next;
      if (marked->key != NULL) free(marked->key);
      if (marked->value != NULL) free(marked->value);
      free(marked);
   }
 
   // free base class object  
   tObject_OnDestroy(tmp);
}
//-----------------------------------------------------------------------------------------
//==[initializer]===========================================================================
void tXMLNode_Init(tXMLNode *self) {
   tObject_Init(&(self->_));

   self->node_type = tXML_UNINITIALIZED;
   self->tag = NULL;
   self->tag_len = 0;
   self->owner = NULL;
   self->parent = NULL;
   self->eldest = NULL;
   self->sibling = NULL;
   self->attr_list = NULL; 
   self->text = NULL;
   self->text_len = 0;;
}
//-----------------------------------------------------------------------------------------
//==[public functions]=====================================================================
void tXMLNode_AttachChild(tXMLNode *self, tXMLNode *node) {
   assert(self != NULL);
   assert(node != NULL);

   if (self->eldest == NULL) {
      self->eldest = node;
   } else {
      tXMLNode *child = self->eldest;
      while (child->sibling != NULL) child = child->sibling;
      child->sibling = node;
   }
}
//-----------------------------------------------------------------------------------------
void tXMLNode_PrintAttr(tXMLNode *self, int tab) {
   tXMLAttr *item;
   //......................................................................................
   void PrintSingleAttr(tXMLAttr *attr, int tab) {
      // print indentation
      for (int i = 0; i < tab; i++) printf("   ");
      printf("(a) %.*s: %.*s\n", attr->key_len, attr->key, attr->value_len, attr->value);
   }
   //......................................................................................
   if (self == NULL) return;
   if (self->attr_list == NULL) return;

   item = self->attr_list;
   while (item != NULL) {
      PrintSingleAttr(item, tab);
      item = item->next;
   }
}
//-----------------------------------------------------------------------------------------
// this print function is for debugging
// it trims off TEXT strings that are just whitespace to remove clutter
void tXMLNode_PrintNode(tXMLNode *self, int tab) {
   if (self == NULL) return;

   if (self->node_type == tXML_NODE) {
      for (int i = 0; i < tab; i++) printf("   ");
      // print tag
      printf("(e) %s\n", self->tag);
      tXMLNode_PrintAttr(self, tab+1);
   
      tXMLNode *node = self->eldest;
      while (node != NULL) {
         tXMLNode_PrintNode(node, tab+1);
         node = node->sibling;
      }
   } else {
      char *text = self->text;
      int text_len = self->text_len;

      //trim whitespaces left and right to avoid clutter
      while (text_len > 0) {
         if (*text <= ' ') { 
            text++;
            text_len--;
            continue;
         }
         if (text[text_len-1] <= ' ') {
            text_len--;
            continue;
         }
         break;
      }
      if (text_len > 0) {
         for (int i = 0; i < tab; i++) printf("   ");
         printf("(t) %.*s\n", text_len, text);
      }
   }
}
//-----------------------------------------------------------------------------------------

