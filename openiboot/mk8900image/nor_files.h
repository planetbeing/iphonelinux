#ifndef NOR_FILES_H
#define NOR_FILES_H

#include "common.h"
#include "8900.h"
#include "img2.h"
#include "lzssfile.h"

#ifdef __cplusplus
extern "C" {
#endif
	AbstractFile* openAbstractFile(AbstractFile* file);
	AbstractFile* duplicateAbstractFile(AbstractFile* file, AbstractFile* backing);
	AbstractFile* duplicateAbstractFileWithCertificate(AbstractFile* file, AbstractFile* backing, AbstractFile* certificate);
#ifdef __cplusplus
}
#endif

#endif

