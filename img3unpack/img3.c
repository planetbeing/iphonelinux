/*
 * cmw
 *
 * greets to dre/wizdaz/pumpkin/saurik/
 *  
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "img3.h"

const char *str_magic(unsigned int magic)
{
	static char str[5] = { 0, 0, 0, 0, 0 };
  magic = ntohl(magic);
  memcpy(str, (void *)&magic, 4);

	return str;
}

void dumpImg3Header(const Img3* header)
{
	printf("=======================================\n");
	printf("Header size: %d\n", sizeof(*header));

	printf("Magic:       0x%x (%s)\n", header->magic, str_magic(header->magic));
	printf("Length:      %u\n", header->dataLenPad);
	printf("Unknown:     %u\n", header->u1);
	printf("Cert Offset: %u\n", header->certOffset);
	printf("Name:        0x%x (%s)\n", header->name, str_magic(header->name));
	printf("Type:        0x%x (%s)\n", header->type, str_magic(header->type));
	printf("nameOffset:  %u\n", header->nameOffset);
	printf("dataLen:     %u\n", header->dataLen);
}
void dumpImg3Footer(const Img3_footer2* footer)
{
	printf("=======================================\n");
	printf("Footer size: %d\n", sizeof(*footer));

	printf("Block Size: %u bytes\n", footer->blockSize);
	printf("Signature:  %u bytes\n", footer->sigLen);
	printf("Cert Head:  0x%x (%s)\n", footer->cert_magic, str_magic(footer->cert_magic));
	printf("Cert Size:  %u\n", footer->cert_size);
	printf("Certficate: %u bytes\n", sizeof(footer->cert));

}
int main(int argc, char *argv[]) {

	Img3 *pHeaderImg3 = NULL;
	Img3_footer1 *pFooterImg3_1 = NULL;
	Img3_footer2 *pFooterImg3_2 = NULL;
	dataFlag *dF = NULL;

	FILE *fw = NULL, *unpack = NULL;
	unsigned long dataSize = 0;
	char *fBuf = NULL, *name = NULL;
	int skip_header =0, dataLen = 0, offSet=0, footerSize = 0;

	if(argc != 3) {
			printf("Syntax: img3 file.img3 outfile.img3\n");
			return 0;
	}
	pHeaderImg3 = (Img3 *)malloc(sizeof(Img3));
	pFooterImg3_1 = (Img3_footer1 *)malloc(sizeof(Img3_footer1));
	pFooterImg3_2 = (Img3_footer2 *)malloc(sizeof(Img3_footer2));

	dF = (dataFlag *)malloc(sizeof(dataFlag));

	if(!(fw = fopen(argv[1], "rb")))
		return -1;

	fseek(fw, 0, SEEK_END);
	dataSize = ftell(fw);
	fseek(fw, 0, SEEK_SET);

	fBuf = malloc(dataSize);
	if(fread(fBuf, 1, dataSize, fw) != dataSize)
		return -1;
	fclose(fw);

	memcpy(pHeaderImg3, fBuf, sizeof(*pHeaderImg3)); 
	if(pHeaderImg3->magic == IMG3_MAGIC_UC || pHeaderImg3->magic == IMG3_MAGIC_LC) {
		dumpImg3Header(pHeaderImg3);
	} else {
		printf("Invalid Img3 file\n");
		return -1;
	}

	dataLen = (((pHeaderImg3->dataLen >> 2) + !!(pHeaderImg3->dataLen & 3)) << 2);
  memcpy(pFooterImg3_1, fBuf + sizeof(Img3) + dataLen, sizeof(*pFooterImg3_1));

  if (pFooterImg3_1->magic == IMG3_FOOTER_MAGIC)
		printf("Header:      %s\n", str_magic(pFooterImg3_1->magic));
  else
		skip_header=1;

	if(!skip_header) {
		name = calloc(1,pFooterImg3_1->name_len+1);
		memcpy(name, fBuf + sizeof(Img3) + pHeaderImg3->nameOffset + 4 , pFooterImg3_1->name_len);
		printf("Name:        %s\n", name);
		dataLen = (((pFooterImg3_1->name_len >> 2) + !!(pFooterImg3_1->name_len & 3)) << 2);
		memcpy(dF, fBuf + sizeof(Img3) + pHeaderImg3->nameOffset + dataLen + sizeof(pFooterImg3_1->name_len), sizeof(*dF));
		offSet = sizeof(*dF);
		while(offSet < pHeaderImg3->dataLenPad) {
			printf("Flag:        0x%x (%s)\n", dF->name, str_magic(dF->name));
      if(dF->name == END_FLAG_HSHS) {
				offSet += - 16;
				memcpy(pFooterImg3_2, fBuf + sizeof(Img3) + pHeaderImg3->nameOffset + dataLen + sizeof(pFooterImg3_1->name_len) + offSet, sizeof(*pFooterImg3_2));
				break;
			}
			memcpy(dF, fBuf + sizeof(Img3) + pHeaderImg3->nameOffset + dataLen + sizeof(pFooterImg3_1->name_len) + offSet, sizeof(*dF));
		  offSet += sizeof(*dF);
		}
	} else {
    memcpy(dF, (fBuf + sizeof(Img3) + pHeaderImg3->nameOffset) - 12 , sizeof(*dF));
		offSet = sizeof(*dF);
    while(offSet < pHeaderImg3->dataLenPad) {
    	printf("Flag:        0x%x (%s)\n", dF->name, str_magic(dF->name));
			printf("Offset:      %d\n", offSet);
      if(dF->name == END_FLAG_HSHS) {
				offSet += - 16;
				memcpy(pFooterImg3_2, (fBuf + sizeof(Img3) + pHeaderImg3->nameOffset) - 12 + offSet, sizeof(*pFooterImg3_2));
				break;
			}
			memcpy(dF, (fBuf + sizeof(Img3) + pHeaderImg3->nameOffset) - 12 + offSet , sizeof(*dF));
			offSet += sizeof(*dF);
		}
	}
	
	dumpImg3Footer(pFooterImg3_2);

  if(!(unpack = fopen(argv[2], "wb")))
			return -1;

	if(fwrite(fBuf + sizeof(Img3), 1, pHeaderImg3->dataLen, unpack) != pHeaderImg3->dataLen)
		return -1;
	fclose(unpack);

	return 0;
}
