#include "openiboot.h"
#include "hfs/common.h"
#include "hfs/bdev.h"
#include "hfs/fs.h"
#include "util.h"

int HasFSInit = FALSE;

int fs_setup() {
	if(HasFSInit)
		return 0;

	bdev_setup();

	HasFSInit = TRUE;

	return 0;
}

