#ifndef __DRIVER_L2_H__
#define __DRIVER_L2_H__

#include "driver_l1.h"
#include "driver_l2_cfg.h"


// Driver layer functions
extern void drv_l2_init(void);

#if (defined _DRV_L2_EXT_RTC) && (_DRV_L2_EXT_RTC == 1)
typedef struct
{
    INT32S tm_sec;  /* 0-59 */
    INT32S tm_min;  /* 0-59 */
    INT32S tm_hour; /* 0-23 */
    INT32S tm_mday; /* 1-31 */
    INT32S tm_mon;  /* 1-12 */
    INT32S tm_year;
    INT32S tm_wday; /* 0-6 Sunday,Monday,Tuesday,Wednesday,Thursday,Friday,Saturday */
}EXTERNAL_RTC_TIME;

typedef struct
{
    INT32S tm_min;  	/* 0-59 */
    INT32S tm_hour; 	/* 0-23 */
    INT32S tm_weekday;	/* 0- 6 */	//0 means sunday.
}EXTERNAL_RTC_ALRAM;

extern void drv_l2_external_rtc_init(void);
//==================================//
//function: drv_l2_rtc_first_power_on_check
//return: 1 means i2c first power on,
//==================================//
extern INT32S drv_l2_rtc_first_power_on_check(void);
extern INT32S drv_l2_rtc_total_set(EXTERNAL_RTC_TIME *ptr);
extern INT32S drv_l2_rtc_total_get(EXTERNAL_RTC_TIME *ptr);

#if DRV_L2_EXT_ALAEM_EN
extern INT32S drv_l2_rtc_alarm_time_set(EXTERNAL_RTC_ALRAM *ptr);
extern INT32S drv_l2_rtc_alarm_time_get(EXTERNAL_RTC_ALRAM *ptr);
#endif
#endif

// system control function
#define MAX_SYSTEM_TIMER		7
extern void sys_init_timer(void);
extern INT32S sys_set_timer(void* msg_qsend, void* msg_q_id, INT32U msg_id, INT32U timerid, INT32U interval);
extern INT32S sys_kill_timer(INT32U timerid);
extern INT32S sys_kill_all_timer(void);

extern INT32S sys_registe_timer_isr(void (*TimerIsrFunction)(void));
extern INT32S sys_release_timer_isr(void (*TimerIsrFunction)(void));
extern INT32S sys_128Hz_timer_init(void);
// ad key
#define AD_KEY_1   0
#define AD_KEY_2   1
#define AD_KEY_3   2
#define AD_KEY_4   3
#define AD_KEY_5   4
#define AD_KEY_6   5
#define AD_KEY_7   6
#define AD_KEY_8   7

typedef enum {
	RB_KEY_DOWN,
	RB_KEY_UP,
	RB_KEY_REPEAT,
	MAX_KEY_TYPES
}KEY_TYPES_ENUM;

extern void ad_key_initial(void);
extern void ad_key_uninitial(void);
extern INT32U ad_key_get_key(void);
extern void ad_key_set_repeat_time(INT8U start_repeat, INT8U repeat_count);
extern void ad_key_get_repeat_time(INT8U *start_repeat, INT8U *repeat_count);

/* USB Host extern function */

#define USBH_Err_Success		0
#define USBH_Err_Fail			-1
#define USBH_Err_ResetByUser	100
#define USBHPlugStatus_None		0
#define USBHPlugStatus_PlugIn	1
#define USBHPlugStatus_PlugOut	2
#define USBH_Err_NoMedia		3

typedef struct
{
	INT8U storage_id;
	INT8U plug_status;
} DRV_PLUG_STATUS_ST;

typedef enum
{
	DRV_FS_DEV_CF=0,
	DRV_FS_DEV_NAND,
	DRV_FS_DEV_SDMS,
	DRV_FS_DEV_USBH,
	DRV_FS_DEV_USBD,
	DRV_FS_DEV_MAX
}DRV_FS_DEV_ID_ENUM;

typedef enum
{
	DRV_PLGU_OUT,
	DRV_PLGU_IN
}DRV_STORAGE_PLUG_STATUS_ENUM;

//extern void   turnkey_usbh_detection_register(void* msg_qsend,void* msgq_id,INT32U msg_id);
extern INT32U usbh_detection_init(void);
extern void usbh_isr(void);
extern INT8U  usbh_mount_status(void);

extern INT32S MSDC_Write2A(INT32U StartLBA, INT32U * StoreAddr, INT32U SectorCnt, INT32U LUN);
extern INT32S MSDC_Read28(INT32U StartLBA, INT32U* StoreAddr, INT32U SectorCnt, INT32U LUN);
extern INT32S DrvUSBHMSDCInitial(void);
extern INT32S DrvUSBHLUNInit(INT32U LUN);
extern INT32U DrvUSBHGetSectorSize(void);
extern INT32U DrvUSBHGetCurrent(void);
extern INT32S DrvUSBHMSDCUninitial(void);
extern INT32S drv_l2_usb_to_ur_send(INT8U *Buf, INT32U len);
extern INT32S drv_l2_usb_to_ur_get(INT8U* Buf, INT32U *len);
extern void usbh_pwr_reset_register(void (*fp_pwr_reset)(void));
/*Nand Flash Driver */


/* Nand COnfiguration information and structure should share with XD card */
#ifndef _NAND_CONFIG_H_
#define _NAND_CONFIG_H_
#define NAND_MLC				0xf1
#define NAND_SLC_LARGE_BLK		0xf2
#define NAND_SLC_SMALL_BLK		0xf3
#define NAND_ALL				0xf4
#define DIRECT_READ_UNSUPPORT	0xa5
#define DIRECT_READ_SUPPORT		0x5a
#define APP_AREA_WRITE_ENABLE	0xa5
#define APP_AREA_WRITE_DISABLE	0x5a


//#define NAND_DRIVER_SPEEDUP	// Use additional ram to speed up read/write 
//#define NAND_DRIVER_DIRECT_READ_SUPPORT
#define NAND_DRIVER_BOOTAREA_WRITE_ENABLE
#define NAND_DRIVER_APPAREA_WRITE_ENABLE

/*######################################################
#												 		#
#	   Nand Flash Blocks Assignation Configuration		#
#														#
 ######################################################*/
#define APP_AREA_START			11	/* Start Physical Block Address */
#define APP_AREA_SIZE			0x40 	/* APP Area Size|Block numbers. */
#define APP_AREA_SPARE_START	(APP_AREA_START + APP_AREA_SIZE)
#define APP_AREA_SPARE_PERCENT  4
#define	APP_AREA_SPARE_SIZE		(APP_AREA_SIZE/APP_AREA_SPARE_PERCENT)	/* Spare area size */
#define DATA_AREA_START			(APP_AREA_SPARE_START + APP_AREA_SPARE_SIZE)

/*######################################################
#												 		#
#	    Nand Flash Spec/Type Configuration	        	#
#														#
 ######################################################*/
//#define HY27US08121A
//#define HY27UT084G2A
//#define K9G8G08U0M
//#define K9G4G08U0A
//#define HY27UT088G2A 
//#define K9K8G08U0M
//#define M29F2G08AAC
#define SUPPORTALL

#if defined SUPPORTALL
	#define CHIP_ID		 	0xffffffff
	#define BLK_NUM		 	16384		// blocks per nand
	#define BLK_PAGE_NUM 	128			// pages  per block
	#define PAGE_SIZE	 	4096		// bytes  per page
	#define SPARE_SIZE	 	128
	#define	NF_TYPE			NAND_ALL
#elif defined HY27UT084G2A
	#define CHIP_ID		 	0xdcada514
	#define BLK_NUM		 	2048		// blocks per nand
	#define BLK_PAGE_NUM 	128			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_MLC
#elif defined	K9G8G08U0M
	#define CHIP_ID		 	0xD3EC2514
	#define BLK_NUM		 	4096		// blocks per nand
	#define BLK_PAGE_NUM 	128			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_MLC	
#elif defined	K9K8G08U0M
	#define CHIP_ID		 	0xDCADA514
	#define BLK_NUM		 	8192		// blocks per nand
	#define BLK_PAGE_NUM 	64			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_SLC_LARGE_BLK	
#elif defined	K9F2G08U0M
	#define CHIP_ID		 	0xDAEC1580
	#define BLK_NUM		 	2048		// blocks per nand
	#define BLK_PAGE_NUM 	64			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_SLC_LARGE_BLK	
#elif defined	HY27UT088G2A
	#define CHIP_ID		 	0xD3ADA514
	#define BLK_NUM		 	4096		// blocks per nand
	#define BLK_PAGE_NUM 	128			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_MLC
#elif defined 	HY27US08121A
	#define CHIP_ID		 	0x76AD76AD
	#define BLK_NUM		 	4096		// blocks per nand
	#define BLK_PAGE_NUM 	32			// pages  per block
	#define PAGE_SIZE	 	512			// bytes  per page
	#define SPARE_SIZE	 	16
	#define	NF_TYPE			NAND_SLC_SMALL_BLK	
#elif defined 	M29F2G08AAC
	#define CHIP_ID		 	0xDA2C1580
	#define BLK_NUM		 	2048		// blocks per nand
	#define BLK_PAGE_NUM 	64			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_SLC_LARGE_BLK	

#elif defined 	K9G4G08U0A
	#define CHIP_ID		 	0xDCEC2514
	#define BLK_NUM		 	2048		// blocks per nand
	#define BLK_PAGE_NUM 	128			// pages  per block
	#define PAGE_SIZE	 	2048		// bytes  per page
	#define SPARE_SIZE	 	64
	#define	NF_TYPE			NAND_SLC_LARGE_BLK	


#else
	" Error: NandFlash Type Not Match!! "
#endif

/*######################################################
#												 		#
#	   Nand Flash Driver Bank&Recycle Configuration	    #
#														#
 ######################################################*/

#if (defined NAND_PAGE_TYPE) && (NAND_PAGE_TYPE == NAND_SMALLBLK)
#define BANK_LOGIC_SIZE		200
#define BANK_RECYCLE_SIZE 	20
#else  /*default condition is Large page*/
#define BANK_LOGIC_SIZE		512
#define BANK_RECYCLE_SIZE 	64
#endif

/*######################	Warning		############################
#																	#
#		Don`t modify the content below,otherwise it will			#
# 	be cause driver can`t work normally or can`t work optimumly!	#
#																	#
###################################################################*/
#define BANK_TOTAL_SIZE 	(BANK_LOGIC_SIZE + BANK_RECYCLE_SIZE)
#define BANK_NUM			((BLK_NUM - DATA_AREA_START + BANK_TOTAL_SIZE -1)/BANK_TOTAL_SIZE)

#ifndef NAND_DRIVER_SPEEDUP
	#define MM_NUM			1
#else
	#define MM_NUM			BANK_NUM
#endif
#ifndef NAND_DRIVER_DIRECT_READ_SUPPORT
	#define MP3_MM_TYPE		INT32U
	#define MP3_TT_TYPE		INT32U
	#define DIRECT_READ		DIRECT_READ_UNSUPPORT
#else
	#define MP3_MM_TYPE		MM
	#define MP3_TT_TYPE		TT
	#define DIRECT_READ		DIRECT_READ_SUPPORT
#endif
#define ExchangeAMaxNum		4

#define NAND_READ_ALLOW	    0x1
#define NAND_WRITE_ALLOW	0x2


/* -------------------   Struct Type Define   ----------------- */
typedef struct NF_USER
{
	INT32U	ulSelectID;
	INT16U	uiNandType;
	INT16U 	uiBlkNum;
	INT16U 	uiPageNum;
	INT16U 	uiPageSize;
	INT16U	uiPageSpareSize;	
	INT16U	uiBankNum;
	INT16U	uiBankSize;
	INT16U	uiBankRecSize;	
	INT16U	uiMM_Num;
	INT16U	uiAppStart;
	INT16U 	uiAppSize;
	INT16U 	uiAppSpareStart;
	INT16U  uiAppSparePercent;
	INT16U 	uiAppSpareSize;
	INT16U 	uiDataStart;
	INT16U	uiDirReadSupport;	
}NF_USER_INFO;

#if 0
	typedef struct
	{
		INT16U	wRecycleBlkNum;
		INT16U	wLogicBlkNum;
		INT16U	wMapTableBlk;
		INT16U	wMapTablePage;
		INT16U  wORGBlkNum;
		INT16U  wUsrMarkBlkNum;
	}BANK_INFO;
#else
	typedef struct
	{
		INT16U	wRecycleBlkNum;
		INT16U	wLogicBlkNum;
		INT16U	wMapTableBlk;
		INT16U	wMapTablePage;
		INT16U  wORGBlkNum;
		INT16U  wUsrMarkBlkNum;
		INT16U  wStartBlkNum;
		INT16U  wEndBlkNum;
	}BANK_INFO;
#endif

typedef struct
{
	INT16U	Logic2Physic[BANK_LOGIC_SIZE];
	INT16U	RecycleBlk[BANK_RECYCLE_SIZE];  
}IMM;

typedef union {
	IMM 	Maptable;	
	INT16U  	pMaptable[BANK_TOTAL_SIZE];	
}MM;

typedef struct
{
	INT8U	wPageData[PAGE_SIZE];
	INT8U	wBad_Flag_0;
	INT8U	wCount;
	INT16U	wLogicNum;
	INT8U	wExchangeType;
	INT8U	wBad_Flag_6;
	INT16U  wP2P;	
}WB;

typedef union
{
	WB PageTag;
	INT8U WorkBuffer[(PAGE_SIZE+SPARE_SIZE)]; 
}TT;

typedef struct 	
{
	INT16U	wLogicBlk;
	INT16U	wPhysicBlk;
}L2P; 

typedef struct
{
	INT16U	wBank;
	INT16U	wLogicBlk;
	INT16U	wPhysicBlk;
	INT16U	wCurrentPage;	
	INT8U	wCount;	
	INT16U	wPage2PageTable[BLK_PAGE_NUM];
}EXCHANGE_A_BLK;

typedef struct
{
	INT16U	wBank;
	INT16U	wLogicBlk;
	INT16U	wPhysicBlk;
	INT16U	wCurrentPage;
	INT16U	wCurrentSector;
	INT8U	wCount;	
}EXCHANGE_B_BLK;

typedef struct
{
	INT16U	wMapTableBlk;
	INT16U	wMapTablePage;
}NAND_HEAD;

typedef struct
{
	INT16U	wPhysicBlkNum;
	INT16U	wLogicBlkNum;
}IBT;

extern 	TT   APP_tt;
extern 	ALIGN16 L2P  gL2PTable[0x200];//size=0x200, so ram size= 0x200*sizeof(L2P)=2048<=page size//gL2PTable[APP_AREA_SPARE_SIZE];
extern  INT16U 		 gMaptableChanged[MM_NUM];
//extern  BANK_INFO  	 gBankInfo[BANK_NUM];
extern  EXCHANGE_A_BLK	gExchangeABlock[ExchangeAMaxNum];
extern  ALIGN16 MM		mm[MM_NUM];
extern  ALIGN8 TT		tt;
#if _OPERATING_SYSTEM != _OS_NONE
extern  OS_EVENT * gNand_sem;
extern  OS_EVENT * gNand_PHY_sem;
#endif


extern void Nand_OS_LOCK(void);
extern void Nand_OS_UNLOCK(void);
extern INT32S NandInfoAutoGet(void);
extern void Nand_FS_Offset_Set(INT32U nand_fs_sector_offset);
extern INT32U Nand_FS_Offset_Get(void);
extern void nand_part0_para_set(INT32U offset, INT32U size, INT16U mode);
extern void nand_part1_para_set(INT32U offset, INT32U size, INT16U mode);
extern void nand_part2_para_set(INT32U offset, INT32U size, INT16U mode);
extern INT32U NandAppByteSizeGet(void);
extern INT32U NandCodeByteSizeGet(void);
extern INT32U Nand_part0_Offset_Get(void);
extern INT32U Nand_part0_mode_Get(void);
extern INT32U nand_fs_byte_size_get(void);
extern INT16U GetBadFlagFromNand(INT16U wPhysicBlkNum);
#endif


extern INT16U DrvNand_initial(void);
extern INT16U DrvNand_lowlevelformat(void);
extern INT32U DrvNand_get_Size(void);
extern INT16U DrvNand_write_sector(INT32U , INT16U , INT32U );
extern INT16U DrvNand_read_sector(INT32U , INT16U , INT32U );
extern INT16U DrvNand_flush_allblk(void);
extern INT32U Nand_libraryversion(void);
extern void DrvNand_bchtable_free(void);
extern INT32U DrvNand_bchtable_alloc(void);
extern INT32U DrvNand_BadBlk(INT16U mode);
extern INT32U DrvNand_RecoveryBlk(void);
extern void   DrvNand_Save_info(INT16U *Addr);
extern INT32U DrvNand_Get_info(INT16U *Addr);
extern INT32U NandBootFormat(INT16U format_type);
extern INT32U NandBootFlush(void);
extern INT32U NandBootInit(void);
extern void NandBootEnableWrite(void);
extern void NandBootDisableWrite(void);
extern INT32U NandBootWriteSector(INT32U wWriteLBA, INT16U wLen, INT32U DataBufAddr);
extern INT32U NandBootReadSector(INT32U wReadLBA, INT16U wLen, INT32U DataBufAddr);
extern void SetNandInfo(INT16U app_start,INT16U app_size,INT16U app_Percent,  INT16U data_BankSize, INT16U data_BankRecSize);
extern void Nand_FS_Offset_Set(INT32U nand_fs_sector_offset);
extern INT32U Nand_FS_Offset_Get(void);
extern INT32U NandAppByteSizeGet(void);
extern INT32U NandCodeByteSizeGet(void);
extern INT32U Nand_part0_Offset_Get(void);
extern INT32U Nand_part0_mode_Get(void);
extern INT32U Nand_part1_size_Get(void);

extern INT32U Nand_part1_Offset_Get(void);
extern INT32U Nand_part1_mode_Get(void);
extern INT32U Nand_part1_size_Get(void);

extern INT16U GetBadFlagFromNand(INT16U wPhysicBlkNum);

#ifndef _NAND_L2_DEFINE_    // jandy add
#define _NAND_L2_DEFINE_    // jandy add
//--------------------------------------------------------
#define NAND_ERR_UNKNOW_TYPE						0xfff0
#define	NAND_BAD_BLK_USER_MARK				        0xff42
#define NAND_FIRST_SCAN_BAD_BLk_MARK		    	0xff5a
#define NAND_SAVE_HEAD                              0xff60
#define NAND_USR_MARK_BAD_BLK				        0xff42
#define NAND_START_WRTIE                            0xff55
#define NAND_END_WRTIE                              0xff66
#define NAND_GOOD_BLK					            0xffff
#define NAND_ALL_BIT_FULL				            0xffff
#define NAND_ALL_BIT_FULL_B							0xff
#define NAND_ALL_BIT_FULL_W							0xffff
#define NAND_MAPTAB_UNUSE							0x7fff
#define NAND_EMPTY_BLK								0xffff
#define NAND_GOOD_TAG								0xff
#define	NAND_ORG_BAD_BLK							0xff00
#define NAND_ORG_BAD_TAG							0x00
#define	NAND_USER_BAD_BLK							0xff44
#define	NAND_USER_BAD_TAG							0x44
#define NAND_FIRST_SCAN_BAD_BLK						0xff55
#define NAND_FIRST_SCAN_BAD_TAG						0x55
#define NAND_MAPTABLE_BLK							0xff66		// word
#define NAND_MAPTABLE_TAG							0xff66		// word
#define NAND_SAVE_HEAD_BLK							0xff60
#define NAND_SAVE_HEAD_TAG							0xff60
#define NAND_START_WRITE                    		0x88
#define NAND_END_WRITE                      		0x99
#define NAND_EXCHANGE_A_TAG							0xaa
#define NAND_EXCHANGE_B_TAG							0xbb
#define MAPTAB_WRITE_BACK_N_NEED					0x01
#define MAPTAB_WRITE_BACK_NEED						0x10
#define NAND_FREE_BLK_MARK_BIT						(1<<15)
//#define NF_APP_LOCK                                  0xfffe
#define NF_PHY_WRITE_FAIL                           0x10
#define NF_PHY_READ_FAIL                            0x01
//--------NAND ERROR INFOMATION define--------------------
#ifndef _NF_STATUS
#define _NF_STATUS
typedef enum
{
	NF_OK 						= 0x0000,
	NF_UNKNOW_TYPE				= 0xff1f,
	NF_USER_CONFIG_ERR			= 0xff2f,
	NF_TOO_MANY_BADBLK			= 0xff3f,
	NF_READ_MAPTAB_FAIL			= 0xff4f,
	NF_SET_BAD_BLK_FAIL			= 0xff5f,
	NF_NO_SWAP_BLOCK			= 0xff6f,
	NF_BEYOND_NAND_SIZE			= 0xff7f,
	NF_READ_ECC_ERR				= 0xff8f,
	NF_READ_BIT_ERR_TOO_MUCH    = 0xff9f,
	NF_ILLEGAL_BLK_MORE_2		= 0xffaf,
	NF_BANK_RECYCLE_NUM_ERR		= 0xffbf,
	NF_EXCHAGE_BLK_ID_ERR		= 0xffcf,
	NF_DIRECT_READ_UNSUPPORT	= 0xffdf,
	NF_DMA_ALIGN_ADDR_NEED		= 0xfff0,
	NF_APP_LOCKED				= 0xfff1,
	NF_NO_BUFFERPTR				= 0xfff2,
	NF_FIRST_ALL_BANK_OK        = 0xffef
}NF_STATUS;
#endif

#ifndef _MANAGER_NANDINFO
#define _MANAGER_NANDINFO
typedef struct
{
	INT16U    wPageSectorMask;
	INT16U    wPageSectorSize;
	INT16U    wBlk2Sector;
	INT16U    wBlk2Page;
	INT16U    wPage2Sector;
	INT16U    wSector2Word;
	INT32U    wNandBlockNum;
	INT16U    wPageSize;
	INT16U    wBlkPageNum;
	
	INT16U	  wNandType;
	INT16U	  wBadFlgByteOfs;
	INT16U	  wBadFlgPage1;
	INT16U	  wBadFlgPage2;	
}Manager_NandInfo;
#endif


//for XDC usage start						//Daniel 12/17
PACKED typedef struct
{
	INT8U  	wPageData[512];
	INT32U 	wReserved_Area;
	INT8U 	wDataStatus;
	INT8U 	wBlkStatus;
	INT16U 	wBlkAddr1;
	INT8U   wECC_2[3];
	INT16U 	wBlkAddr2;
	//INT8U 	wBlkAddr3;
	INT8U 	wECC_1[3];
}XB;

typedef union{
	XB PageTag;
	INT8U WorkBuffer[528];
}TX;
//for XDC usage end

//IR
#if (defined _DRV_L2_IR) && (_DRV_L2_IR == 1)
typedef enum {
	IR_KEY_DOWN_TYPE,
	IR_KEY_REPEAT_TYPE,
	MAX_IR_KEY_TYPES
}IR_KEY_TYPES_ENUM;
#endif

#endif //_NAND_L2_DEFINE_


// XD extern define and funnctions
extern INT16U DrvXD_initial(void);
extern INT16U DrvXD_uninit(void);
extern INT32U DrvXD_getNsectors(void);
extern INT16U DrvXD_write_sector(INT32U wWriteLBA, INT16U wLen, INT32U DataBufAddr);
extern INT16U DrvXD_read_sector(INT32U wReadLBA, INT16U wLen, INT32U DataBufAddr);
extern INT16U DrvXD_Flush_RAM_to_XDC(void);		//Daniel 03/11


#if (defined _DRV_L2_IR) && (_DRV_L2_IR == 1)
extern void drv_l2_ir_init(void);
extern INT32S drv_l2_getirkey(void);
#endif

// spi flash driver
extern INT32S SPI_Flash_init(void);
// byte 1 is manufacturer ID,byte 2 is memory type ID ,byte 3 is device ID
extern INT32S SPI_Flash_readID(INT8U* Id);
// sector earse(4k byte)
extern INT32S SPI_Flash_erase_sector(INT32U addr);
// block erase(64k byte)
extern INT32S SPI_Flash_erase_block(INT32U addr);
// chip erase
extern INT32S SPI_Flash_erase_chip(void);
// read a page(256 byte)
extern INT32S SPI_Flash_read_page(INT32U addr, INT8U *buf);
// write a page(256 byte)
extern INT32S SPI_Flash_write_page(INT32U addr, INT8U *buf);
// read n byte
extern INT32S SPI_Flash_read(INT32U addr, INT8U *buf, INT32U nByte);

// Key Scan driver
extern INT32U IO1, IO2, IO3, IO4, IO5, IO6, IO7, IO8;
extern void key_Scan(void);

extern void itouch_init(void);
extern void itouch_reduce_write (INT8U salve_reg_addr, INT8U write_data);
extern INT8U itouch_read_nBytes (INT8U slave_id, INT8U slave_reg_addr, INT8U *data_buf, INT32U data_size);
extern INT8U itouch_reduce_read_nBytes (INT8U slave_reg_addr, INT8U *data_buf, INT32U data_size);
extern INT8U itouch_version_get(void);
extern void itouch_sensitive_set(INT8U level);
extern INT8U itouch_sensitive_get(void);
extern void turnkey_itouch_resource_register(void* msg_qsend,void* msgq_id,INT32U msg_id);


// SD and MMC card access APIs for system
typedef struct {
	INT32U csd[4];
	INT32U scr[2];
	INT32U cid[4];
	INT32U ocr;
	INT16U rca;
} SD_CARD_INFO_STRUCT;
extern INT32S drvl2_sd_init(void);
extern void drvl2_sdc_card_info_get(SD_CARD_INFO_STRUCT *sd_info);
extern INT32S drvl2_sd_bus_clock_set(INT32U limit_speed);
extern INT8U drvl2_sdc_card_protect_get(void);			// Return 0 when writing to SD card is allowed
extern void drvl2_sdc_card_protect_set(INT8U value);	// value: 0=allow to write SD card. Other values=Not allow to write SD card
extern INT16S drvl2_sdc_live_response(void);

// SD and MMC card access APIs for file-system
extern INT32U drvl2_sd_sector_number_get(void);
extern INT32S drvl2_sd_read(INT32U start_sector, INT32U *buffer, INT32U sector_count);
extern INT32S drvl2_sd_write(INT32U start_sector, INT32U *buffer, INT32U sector_count);
extern INT32S drvl2_sd_card_remove(void);
// SD and MMC card access APIs for USB device
extern INT32S drvl2_sd_read_start(INT32U start_sector, INT32U sector_count);
extern INT32S drvl2_sd_read_sector(INT32U *buffer, INT32U sector_count, INT8U wait_flag);	// wait_flag: 0=start data read and return immediately, 1=start data read and return when done, 2=query read result
extern INT32S drvl2_sd_read_stop(void);
extern INT32S drvl2_sd_write_start(INT32U start_sector, INT32U sector_count);
extern INT32S drvl2_sd_write_sector(INT32U *buffer, INT32U sector_count, INT8U wait_flag);	// wait_flag: 0=start data write and return immediately, 1=start data write and return when done, 2=query write result
extern INT32S drvl2_sd_write_stop(void);

extern void rotate_sensor_initial(void);
extern void turnkey_rotate_sensor_resource_register(void* msg_qsend,void* msgq_id,INT32U msg_id);
extern INT32S rotate_sensor_status_get(void);

#if 0//SUPPORT_AI_FM_MODE == CUSTOM_ON
//FM module
extern void FMRev_Tune(INT32U freq);
extern void FMRev_Initial(void);
extern void FM_SearchAll(void);
extern void FMRev_Exit(void);
extern INT32U FM_Current_Frequency_Get(void);
extern void FM_Current_Frequency_Set(INT32U Freq);
extern INT32U FM_Temp_Frequency_Get(void);
extern void FM_Temp_Frequency_Set(INT32U Freq);
extern void FMRev_Volume(INT32U vol);
extern void MUTE_FM_REV(void);
extern void UnMUTE_FM_REV(void);

extern BOOLEAN FMRev_SearchUP(INT32U freq);
extern BOOLEAN FMRev_SearchDOWN(INT32U freq);

extern INT32U FM_Auto_Scan_Temp_Frequency_Get(void);
extern void FM_Auto_Scan_Temp_Frequency_Set(INT32U Freq);

#endif	//#if SUPPORT_AI_FM_MODE == CUSTOM_ON

#endif		// __DRIVER_L2_H__
