#define CREAT_DRIVERLAYER_STRUCT

#include "fsystem.h"
#include "driver_l2.h"

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#if (defined SD_EN) && (SD_EN == 1)                               //

//================================================================//

#define SD_RW_RETRY_COUNT       3

static INT32S SD_Initial(void);
static INT32S SD_Uninitial(void);
static void SD_GetDrvInfo(struct DrvInfo* info);
static INT32S SD_ReadSector(INT32U blkno, INT32U blkcnt, INT32U buf);
static INT32S SD_WriteSector(INT32U blkno, INT32U blkcnt, INT32U buf);
static INT32S SD_Flush(void);
static INT32S SD_WriteStart(INT32U blkno);
static INT32S SD_WriteContinue(INT32U blkcnt, INT32U buf);
static INT32S SD_WriteStop(void);

//--------------------------------------------------
struct Drv_FileSystem FS_SD_driver = {
	"SD",
	DEVICE_READ_ALLOW|DEVICE_WRITE_ALLOW,
	SD_Initial,
	SD_Uninitial,
	SD_GetDrvInfo,
	SD_ReadSector,
	SD_WriteSector,
	SD_Flush,
	SD_WriteStart,
	SD_WriteContinue,
	SD_WriteStop,
};

INT32S SD_Initial(void)
{
    INT32S ret;

	ret = drvl2_sd_init();
	if (ret)
	{
		DBG_PRINT("FS drvl2_sd_init fail\r\n");
	}
	fs_sd_ms_plug_out_flag_reset();
	return ret; 
}

INT32S SD_Uninitial(void)
{
     drvl2_sd_card_remove();
	 return 0;
}

void SD_GetDrvInfo(struct DrvInfo* info)
{
	info->nSectors = drvl2_sd_sector_number_get();
	info->nBytesPerSector = 512;
}

INT32S SD_ReadSector(INT32U blkno, INT32U blkcnt, INT32U buf)
{
	INT32S	ret;
	INT32S	i;

    if (fs_sd_ms_plug_out_flag_get()==1) {return 0xFFFFFFFF;}

	for(i=0; i<SD_RW_RETRY_COUNT; i++) {
		ret = drvl2_sd_read(blkno, (INT32U *) buf, blkcnt);
		if(ret == 0) {
			break;
		} else {
			DBG_PRINT("SD read fail 8\r\n");
		}
	}

#if SUPPORT_STG_SUPER_PLUGOUT == 1
    if(ret != 0) {
        //if(drvl2_sd_read(0, (INT32U *) buf, 1) != 0)
        {
            fs_sd_ms_plug_out_flag_en();
            DBG_PRINT ("============>SUPER PLUG OUT DETECTED<===========\r\n");
        }
    }
#endif

	return ret;
}


INT32S SD_WriteSector(INT32U blkno, INT32U blkcnt, INT32U buf)
{
	INT32S	ret;
	INT32S	i;

    if(fs_sd_ms_plug_out_flag_get() == 1) {return 0xFFFFFFFF;}

	for(i=0; i<SD_RW_RETRY_COUNT; i++)
	{
		ret = drvl2_sd_write(blkno, (INT32U *) buf, blkcnt);
		if(ret == 0) {
			break;
		} else {
			DBG_PRINT("SD write fail 5\r\n");
		}
	}

#if SUPPORT_STG_SUPER_PLUGOUT == 1
    if (ret!=0) {
        if (drvl2_sd_read(0, (INT32U *) buf, 1) != 0) {
            fs_sd_ms_plug_out_flag_en();
            DBG_PRINT ("============>SUPER PLUG OUT DETECTED<===========\r\n");
        }
    }
#endif

	return ret;
}

INT32S SD_Flush(void)
{

    //DBG_PRINT ("+++++++SD1_Flush\r\n");
	return 0;
}


INT32S SD_WriteStart(INT32U blkno)
{
	if(drvl2_sd_write_start(blkno, 0) != 0) {
		return -1;
	}
	return 0;
}


INT32S SD_WriteContinue(INT32U blkcnt, INT32U buf)
{
	if(drvl2_sd_write_sector((INT32U *)buf, blkcnt, 1) != 0) {
		return -1;
	}
	return 0;
}


INT32S SD_WriteStop(void)
{
	if(drvl2_sd_write_stop() != 0) {
		return -1;
	}
	return 0;
}


//=== SD 1 setting ===//
#if (SD_DUAL_SUPPORT==1)

INT32S SD1_Initial(void)
{
	return drvl2_sd1_init();
}

INT32S SD1_Uninitial(void)
{
     drvl2_sd1_card_remove();
	 return 0;
}

void SD1_GetDrvInfo(struct DrvInfo* info)
{
	info->nSectors = drvl2_sd1_sector_number_get();
	info->nBytesPerSector = 512;
}

//read/write speed test function
INT32S SD1_ReadSector(INT32U blkno, INT32U blkcnt, INT32U buf)
{
	INT32S	ret;
	INT32S	i;

    if (fs_sd_ms_plug_out_flag_get()==1) {return 0xFFFFFFFF;}
	for(i = 0; i < SD_RW_RETRY_COUNT; i++)
	{
		ret = drvl2_sd1_read(blkno, (INT32U *) buf, blkcnt);
		if(ret == 0)
		{
			break;
		}
	}
  #if SUPPORT_STG_SUPER_PLUGOUT == 1
    if (ret!=0) 
    {
        //if (drvl2_sd_read(0, (INT32U *) buf, 1)!=0)
        {
            fs_sd_ms_plug_out_flag_en();
            DBG_PRINT ("============>SUPER PLUG OUT DETECTED<===========\r\n");
        }
    }
  #endif
	return ret;
}

INT32S SD1_WriteSector(INT32U blkno, INT32U blkcnt, INT32U buf)
{
	INT32S	ret;
	INT32S	i;

    if (fs_sd_ms_plug_out_flag_get()==1) {return 0xFFFFFFFF;}
	for(i = 0; i < SD_RW_RETRY_COUNT; i++)
	{
		ret = drvl2_sd1_write(blkno, (INT32U *) buf, blkcnt);
		if(ret == 0)
		{
			break;
		}
	}
  #if SUPPORT_STG_SUPER_PLUGOUT == 1
    if (ret!=0) 
    {
        if (drvl2_sd1_read(0, (INT32U *) buf, 1)!=0)
        {
            fs_sd_ms_plug_out_flag_en();
            DBG_PRINT ("============>SUPER PLUG OUT DETECTED<===========\r\n");
        }
    }
  #endif
	return ret;
}

INT32S SD1_Flush(void)
{

    //DBG_PRINT ("+++++++SD1_Flush\r\n");
	return 0;
}

struct Drv_FileSystem FS_SD1_driver = {
	"SD1",
	DEVICE_READ_ALLOW|DEVICE_WRITE_ALLOW,
	SD1_Initial,
	SD1_Uninitial,
	SD1_GetDrvInfo,
	SD1_ReadSector,
	SD1_WriteSector,
	SD1_Flush,
};
#endif

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#endif //(defined SD_EN) && (SD_EN == 1)                          //
//================================================================//

