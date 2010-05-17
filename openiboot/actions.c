#include "actions.h"
#include "openiboot-asmhelpers.h"
#include "arm.h"
#include "lcd.h"
#include "mmu.h"
#include "util.h"
#include "hfs/fs.h"
#include "nand.h"
#include "wdt.h"
#include "dma.h"
#include "radio.h"
#include "syscfg.h"

#define MACH_APPLE_IPHONE 1506

/* this code is adapted from http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html, which is distributed under the BSD license */

/* list of possible tags */
#define ATAG_NONE       0x00000000
#define ATAG_CORE       0x54410001
#define ATAG_MEM        0x54410002
#define ATAG_VIDEOTEXT  0x54410003
#define ATAG_RAMDISK    0x54410004
#define ATAG_INITRD2    0x54420005
#define ATAG_SERIAL     0x54410006
#define ATAG_REVISION   0x54410007
#define ATAG_VIDEOLFB   0x54410008
#define ATAG_CMDLINE    0x54410009
#define ATAG_IPHONE_NAND       0x54411001
#define ATAG_IPHONE_WIFI       0x54411002
#define ATAG_IPHONE_PROX_CAL   0x54411004
#define ATAG_IPHONE_MT_CAL     0x54411005

/* structures for each atag */
struct atag_header {
	uint32_t size; /* length of tag in words including this header */
	uint32_t tag;  /* tag type */
};

struct atag_core {
	uint32_t flags;
	uint32_t pagesize;
	uint32_t rootdev;
};

struct atag_mem {
	uint32_t     size;
	uint32_t     start;
};

struct atag_videotext {
	uint8_t              x;
	uint8_t              y;
	uint16_t             video_page;
	uint8_t              video_mode;
	uint8_t              video_cols;
	uint16_t             video_ega_bx;
	uint8_t              video_lines;
	uint8_t              video_isvga;
	uint16_t             video_points;
};

struct atag_ramdisk {
	uint32_t flags;
	uint32_t size;
	uint32_t start;
};

struct atag_initrd2 {
	uint32_t start;
	uint32_t size;
};

struct atag_serialnr {
	uint32_t low;
	uint32_t high;
};

struct atag_revision {
	uint32_t rev;
};

struct atag_videolfb {
	uint16_t             lfb_width;
	uint16_t             lfb_height;
	uint16_t             lfb_depth;
	uint16_t             lfb_linelength;
	uint32_t             lfb_base;
	uint32_t             lfb_size;
	uint8_t              red_size;
	uint8_t              red_pos;
	uint8_t              green_size;
	uint8_t              green_pos;
	uint8_t              blue_size;
	uint8_t              blue_pos;
	uint8_t              rsvd_size;
	uint8_t              rsvd_pos;
};

struct atag_cmdline {
	char    cmdline[1];
};

struct atag_iphone_nand {
	uint32_t	nandID;
	uint32_t	numBanks;
	int		banksTable[8];
	ExtentList	extentList;
};

#define ATAG_IPHONE_WIFI_TYPE_2G 0
#define ATAG_IPHONE_WIFI_TYPE_3G 1
#define ATAG_IPHONE_WIFI_TYPE_IPOD 2

struct atag_iphone_wifi {
	uint8_t		mac[6];
	uint32_t	calSize;
	uint8_t		cal[];
};

struct atag_iphone_cal_data {
	uint32_t	size;
	uint8_t		data[];
};

struct atag {
	struct atag_header hdr;
	union {
		struct atag_core             core;
		struct atag_mem              mem;
		struct atag_videotext        videotext;
		struct atag_ramdisk          ramdisk;
		struct atag_initrd2          initrd2;
		struct atag_serialnr         serialnr;
		struct atag_revision         revision;
		struct atag_videolfb         videolfb;
		struct atag_cmdline          cmdline;
		struct atag_iphone_nand      nand;
		struct atag_iphone_wifi      wifi;
		struct atag_iphone_cal_data  mt_cal;
	} u;
};

void chainload(uint32_t address) {
	EnterCriticalSection();
	wdt_disable();
	arm_disable_caches();
	mmu_disable();
	CallArm(address);
}

#define tag_next(t)     ((struct atag *)((uint32_t *)(t) + (t)->hdr.size))
#define tag_size(type)  ((sizeof(struct atag_header) + sizeof(struct type)) >> 2)
static struct atag *params; /* used to point at the current tag */

static void setup_core_tag(void * address, long pagesize)
{
	params = (struct atag *)address;         /* Initialise parameters to start at given address */

	params->hdr.tag = ATAG_CORE;            /* start with the core tag */
	params->hdr.size = tag_size(atag_core); /* size the tag */

	params->u.core.flags = 1;               /* ensure read-only */
	params->u.core.pagesize = pagesize;     /* systems pagesize (4k) */
	params->u.core.rootdev = 0;             /* zero root device (typicaly overidden from commandline )*/

	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_ramdisk_tag(uint32_t size)
{
	params->hdr.tag = ATAG_RAMDISK;         /* Ramdisk tag */
	params->hdr.size = tag_size(atag_ramdisk);  /* size tag */

	params->u.ramdisk.flags = 0;            /* Load the ramdisk */
	params->u.ramdisk.size = size;          /* Decompressed ramdisk size */
	params->u.ramdisk.start = 0;            /* Unused */

	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_initrd2_tag(uint32_t start, uint32_t size)
{
	params->hdr.tag = ATAG_INITRD2;         /* Initrd2 tag */
	params->hdr.size = tag_size(atag_initrd2);  /* size tag */

	params->u.initrd2.start = start;        /* physical start */
	params->u.initrd2.size = size;          /* compressed ramdisk size */

	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_mem_tag(uint32_t start, uint32_t len)
{
	params->hdr.tag = ATAG_MEM;             /* Memory tag */
	params->hdr.size = tag_size(atag_mem);  /* size tag */

	params->u.mem.start = start;            /* Start of memory area (physical address) */
	params->u.mem.size = len;               /* Length of area */

	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_cmdline_tag(const char * line)
{
	int linelen = strlen(line) + 1;

	if(!linelen)
		return;                             /* do not insert a tag for an empty commandline */

	params->hdr.tag = ATAG_CMDLINE;         /* Commandline tag */
	params->hdr.size = (sizeof(struct atag_header) + linelen + 1 + 4) >> 2;

	strcpy(params->u.cmdline.cmdline,line); /* place commandline into tag */

	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_wifi_tags()
{
	uint8_t* mac;
	uint32_t calSize;
	uint8_t* cal;

#ifdef CONFIG_IPOD
	return;
#else
	if(radio_nvram_get(2, &mac) < 0)
		return;

	if((calSize = radio_nvram_get(1, &cal)) < 0)
		return;
#endif

	memcpy(&params->u.wifi.mac, mac, 6);
	params->u.wifi.calSize = calSize;
	memcpy(params->u.wifi.cal, cal, calSize);

	params->hdr.tag = ATAG_IPHONE_WIFI;         /* iPhone NAND tag */
	params->hdr.size = (sizeof(struct atag_header) + sizeof(struct atag_iphone_wifi) + calSize + 4) >> 2;
	params = tag_next(params);              /* move pointer to next tag */
}

static void setup_prox_tag()
{
#ifdef CONFIG_3G
	uint8_t* prox_cal;
	int prox_cal_size;

	prox_cal = syscfg_get_entry(SCFG_PxCl, &prox_cal_size);
	if(!prox_cal)
	{
		return;
	}

	params->u.mt_cal.size = prox_cal_size;
	memcpy(params->u.mt_cal.data, prox_cal, prox_cal_size);

	params->hdr.tag = ATAG_IPHONE_PROX_CAL;
	params->hdr.size = (sizeof(struct atag_header) + sizeof(struct atag_iphone_cal_data) + prox_cal_size + 4) >> 2;
	params = tag_next(params);              /* move pointer to next tag */

	bufferPrintf("Proximity calibration data installed.\r\n");
#endif
}

static void setup_mt_tag()
{
#ifndef CONFIG_IPHONE
	uint8_t* cal;
	int cal_size;

	cal = syscfg_get_entry(SCFG_MtCl, &cal_size);
	if(!cal)
	{
		return;
	}

	params->u.mt_cal.size = cal_size;
	memcpy(params->u.mt_cal.data, cal, cal_size);

	params->hdr.tag = ATAG_IPHONE_MT_CAL;
	params->hdr.size = (sizeof(struct atag_header) + sizeof(struct atag_iphone_cal_data) + cal_size + 4) >> 2;
	params = tag_next(params);              /* move pointer to next tag */

	bufferPrintf("Multi-touch calibration data installed.\r\n");
#endif
}

static int rootfs_partition = 0;
static char* rootfs_filename = NULL;

#ifndef NO_HFS
static void setup_iphone_nand_tag()
{
	int i;

	if(!rootfs_filename)
		return;

	ExtentList* extentList = fs_get_extents(rootfs_partition, rootfs_filename);
	if(!extentList)
		return;

	params->u.nand.nandID = (nand_get_geometry())->DeviceID;
	params->u.nand.numBanks = (nand_get_geometry())->banksTotal;
	memcpy(&params->u.nand.banksTable, (nand_get_geometry())->banksTable, sizeof(params->u.nand.banksTable));
	params->u.nand.extentList.numExtents = extentList->numExtents;
	for(i = 0; i < extentList->numExtents; i++) {
		params->u.nand.extentList.extents[i].startBlock = extentList->extents[i].startBlock;
		params->u.nand.extentList.extents[i].blockCount = extentList->extents[i].blockCount;
	}
	free(extentList);

	params->hdr.tag = ATAG_IPHONE_NAND;         /* iPhone NAND tag */
	params->hdr.size = tag_size(atag_iphone_nand);
	params = tag_next(params);              /* move pointer to next tag */
}
#endif

static void setup_end_tag()
{
	params->hdr.tag = ATAG_NONE;            /* Empty tag ends list */
	params->hdr.size = 0;                   /* zero length */
}

static void* kernel = NULL;
static uint32_t kernelSize;
static void* ramdisk = NULL;
static uint32_t ramdiskSize;
static uint32_t ramdiskRealSize;

void set_ramdisk(void* location, int size) {
	if(ramdisk)
		free(ramdisk);

	// the gzip file format places the uncompressed length in the last four bytes of the file. Read it and calculate the size in KB.
	ramdiskRealSize = ((*((uint32_t*)((uint8_t*)location + size - sizeof(uint32_t)))) + 1023) / 1024;

	ramdiskSize = size;
	ramdisk = malloc(size);
	memcpy(ramdisk, location, size);
}

void set_kernel(void* location, int size) {
	if(kernel)
		free(kernel);

	kernelSize = size;
	kernel = malloc(size);
	memcpy(kernel, location, size);
}

void set_rootfs(int partition, const char* fileName) {
	rootfs_partition = partition;
	if(rootfs_filename)
		free(rootfs_filename);
	rootfs_filename = strdup(fileName);
}

#define INITRD_LOAD 0x06000000

static void setup_tags(struct atag* parameters, const char* commandLine)
{
	setup_core_tag(parameters, 4096);       /* standard core tag 4k pagesize */
	setup_mem_tag(MemoryStart, 0x08000000);    /* 128Mb at 0x00000000 */
	if(ramdisk != NULL && ramdiskSize > 0) {
		setup_ramdisk_tag(ramdiskRealSize);
		setup_initrd2_tag(INITRD_LOAD, ramdiskSize);
	}
	setup_cmdline_tag(commandLine);
	setup_prox_tag();
	setup_mt_tag();
	setup_wifi_tags();
#ifndef NO_HFS
	setup_iphone_nand_tag();
#endif
	setup_end_tag();                    /* end of tags */
}

void boot_linux(const char* args) {
	uint32_t exec_at = (uint32_t) kernel;
	uint32_t param_at = exec_at - 0x2000;
	setup_tags((struct atag*) param_at, args);

	uint32_t mach_type = MACH_APPLE_IPHONE;

	EnterCriticalSection();
	dma_shutdown();
	wdt_disable();
	arm_disable_caches();
	mmu_disable();

	/* FIXME: This overwrites openiboot! We make the semi-reasonable assumption
	 * that this function's own code doesn't reside in 0x0100-0x1100 */

	int i;

	for(i = 0; i < ((ramdiskSize + sizeof(uint32_t) - 1)/sizeof(uint32_t)); i++) {
		((uint32_t*)INITRD_LOAD)[i] = ((uint32_t*)ramdisk)[i];
	}

	for(i = 0; i < (0x1000/sizeof(uint32_t)); i++) {
		((uint32_t*)0x100)[i] = ((uint32_t*)param_at)[i];
	}

	asm (	"MOV	R4, %0\n"
		"MOV	R0, #0\n"
		"MOV	R1, %1\n"
		"MOV	R2, %2\n"
		"BX	R4"
		:
		: "r"(exec_at), "r"(mach_type), "r"(0x100)
	    );
}

#ifndef NO_HFS
void boot_linux_from_files()
{
	int size;

	bufferPrintf("Loading kernel...\r\n");

	size = fs_extract(1, "/zImage", (void*) 0x09000000);
	if(size < 0)
	{
		bufferPrintf("Cannot find kernel.\r\n");
		return;
	}

	set_kernel((void*) 0x09000000, size);

	bufferPrintf("Loading initrd...\r\n");

	size = fs_extract(1, "/android.img.gz", (void*) 0x09000000);
	if(size < 0)
	{
		bufferPrintf("Cannot find ramdisk.\r\n");
		return;
	}

	set_ramdisk((void*) 0x09000000, size);

	bufferPrintf("Booting Linux...\r\n");

	boot_linux("console=tty root=/dev/ram0 init=/init rw");
}
#endif
