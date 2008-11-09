#include <arpa/inet.h>

struct Img3 {
  unsigned int magic;
  unsigned int dataLenPad;
  unsigned int u1; // offSet to 20 byte footer
  unsigned int certOffset;
  unsigned int name;
  unsigned int type;
  unsigned int nameOffset;
  unsigned int dataLen;
} typedef Img3;

#define IMG3_MAGIC_UC   0x496d6733
#define IMG3_MAGIC_LC   0x696D6733

struct Img3_footer1 {
	unsigned int magic;
	unsigned int u1;
	unsigned int u2;
  unsigned int name_len;
} typedef Img3_footer1;

struct dataFlag {
  unsigned int name;
  unsigned int u5;
  unsigned int u6;
  unsigned int u7;
} typedef dataFlag;

struct Img3_footer2 {
	unsigned int name;
	unsigned int blockSize;
	unsigned int sigLen;
	unsigned char sig[0x80];
	unsigned int cert_magic;
	unsigned int cert_size;
	unsigned char cert[0xc18];
} typedef Img3_footer2;

#define IMG3_FOOTER_MAGIC		0x56455253
#define START_FLAG_SEPO		  0x5345504f
#define OPTION_BORD					0x424f5244
#define END_FLAG_HSHS			  0x53485348
