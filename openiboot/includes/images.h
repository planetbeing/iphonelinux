#include "openiboot.h"
#include "util.h"

#define Img2Signature fourcc("Img2")
#define IMG2Signature 0x494D4732

typedef struct Img2Header {
        uint32_t signature;        /* 0x0 */
        uint32_t imageType;        /* 0x4 */
        uint16_t unknown1;         /* 0x8 */
        uint16_t security_epoch;   /* 0xa */
        uint32_t flags1;           /* 0xc */
        uint32_t dataLenPadded;    /* 0x10 */
        uint32_t dataLen;          /* 0x14 */
        uint32_t index;            /* 0x18 */
        uint32_t flags2;           /* 0x1c */ /* 0x01000000 has to be unset */
        uint8_t  dataHash[0x40];   /* 0x20 */
        uint32_t unknownLength;    /* 0x60 */ /* some sort of length field? */
        uint32_t header_checksum;  /* 0x64 */ /* standard crc32 on first 0x64 bytes */
        uint32_t unknownChecksum;  /* 0x68 */
        uint8_t  unknown[0x374];   /* 0x6C */
	uint8_t  hash[0x20];       /* 0x3E0 */
} Img2Header;

typedef struct IMG2 {
        uint32_t signature;             /* 0x0 */
        uint32_t segmentSize;           /* 0x4 */
        uint32_t dataStart;             /* 0x8 */
        uint32_t imagesStart;           /* 0xc */
        uint32_t unk0;                  /* 0x10 */
        uint32_t unk1;                  /* 0x14 */
        uint32_t unk2;                  /* 0x18 */
        uint8_t  unknown5[0x3E4];       /* 0x1c */
} IMG2;

#define IMG3_MAGIC 0x496d6733
#define IMG3_DATA_MAGIC 0x44415441
#define IMG3_VERS_MAGIC 0x56455253
#define IMG3_SEPO_MAGIC 0x5345504f
#define IMG3_SCEP_MAGIC 0x53434550
#define IMG3_BORD_MAGIC 0x424f5244
#define IMG3_BDID_MAGIC 0x42444944
#define IMG3_SHSH_MAGIC 0x53485348
#define IMG3_CERT_MAGIC 0x43455254
#define IMG3_KBAG_MAGIC 0x4B424147

#define IMG3_SIGNATURE IMG3_MAGIC

typedef struct AppleImg3Header {
	uint32_t magic;
	uint32_t size;
	uint32_t dataSize;
}__attribute__((__packed__)) AppleImg3Header;

typedef struct AppleImg3RootExtra {
	uint32_t shshOffset;
	uint32_t name;
}__attribute__((__packed__)) AppleImg3RootExtra;

typedef struct AppleImg3RootHeader {
	AppleImg3Header base;
	AppleImg3RootExtra extra;
}__attribute__((__packed__)) AppleImg3RootHeader;

typedef struct AppleImg3KBAGHeader {
  uint32_t key_modifier;		// key modifier, can be 0 or 1 	
  uint32_t key_bits;			// number of bits in the key, can be 128, 192 or 256 (it seems only 128 is supported in current iBoot)
} AppleImg3KBAGHeader;

typedef struct Image {
	struct Image* next;
	uint32_t type;
	uint32_t offset;
	uint32_t index;
	uint32_t length;
	uint32_t padded;
	uint8_t dataHash[0x40];
	int hashMatch;
} Image;

typedef struct ImageDataList {
	uint32_t type;
	void* data;
	struct ImageDataList* next;
} ImageDataList;

static inline uint32_t fourcc(char* code) {
	return (code[0] << 24) | (code[1] << 16) | (code[2] << 8) | code[3];
}

static inline void print_fourcc(uint32_t code) {
	bufferPrintf("%c%c%c%c", (code >> 24) & 0xFF, (code >> 16) & 0xFF, (code >> 8) & 0xFF, code & 0xFF);
}

int images_setup();
void images_list();
Image* images_get(uint32_t type);
void images_release();
void images_duplicate(Image* image, uint32_t type, int index);
void images_duplicate_at(Image* image, uint32_t type, int index, int offset);
void images_from_template(Image* image, uint32_t type, int index, void* dataBuffer, unsigned int len, int encrypt);
void images_erase(Image* image);
void images_write(Image* image, void* data, unsigned int length, int encrypt);
unsigned int images_read(Image* image, void** data);
int images_verify(Image* image);
void images_append(void* data, int len);
void images_rewind();
void* images_inject_img3(const void* img3Data, const void* newData, size_t newDataLen);
void images_install(void* newData, size_t newDataLen);
void images_uninstall();
void images_change_type(const void* img3Data, uint32_t type);
