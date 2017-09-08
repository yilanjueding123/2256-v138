#ifndef __GPLIB_CFG_H__
#define __GPLIB_CFG_H__

    // Memory management module
    #define GPLIB_MEMORY_MANAGEMENT_EN		1
		#define C_MM_IRAM_LINK_NUM			20
		#define C_MM_16BYTE_NUM				0
		#define C_MM_64BYTE_NUM				0
		#define C_MM_256BYTE_NUM			0
		#define C_MM_1024BYTE_NUM			0
		#define C_MM_4096BYTE_NUM			0

    // File system module
    #define	GPLIB_FILE_SYSTEM_EN			1

    // JPEG image encoding/decoding module
    #define GPLIB_JPEG_ENCODE_EN			1
    #define GPLIB_JPEG_DECODE_EN			0

    // Progressive JPEG image decoding module
    #define GPLIB_PROGRESSIVE_JPEG_EN		0

    // GPZP image decoding module
    #define GPLIB_GPZP_EN					0

    // GIF image decoding module
    #define GPLIB_GIF_EN                    0

    // BMP image decoding module
    #define GPLIB_BMP_EN                    0

    // PPU control module
    #define GPLIB_PPU_EN                   0

    // SPU control module
    #define GPLIB_SPU_EN					0

    // Post processing: face detection
    #define GPLIB_FACE_DETECTION            0

    // SPI flash read/write module
    #define GPLIB_NVRAM_SPI_FLASH_EN        0

    // Print and get string
    #define GPLIB_PRINT_STRING_EN           1
    #define PRINT_BUF_SIZE                  512

    #define GPLIB_CALENDAR_EN               1

    #define SUPPORT_MIDI_LOAD_FROM_NVRAM   0

    #define SUPPORT_MIDI_READ_FROM_SDRAM    0
    
    #define SUPPORT_MIDI_PLAY_WITHOUT_TASK	0
    
    /*=== USBH detection mode ===*/
    #define USBH_DP_D   	0x0001
    #define USBH_GPIO_D 	0x0002
    #define USBH_GPIO_D_1	0x0003
    #define USBH_DETECTION_MODE USBH_DP_D

  #ifndef _FS_DEFINE_
  #define _FS_DEFINE_
    #define FS_NAND1    0
    #define FS_NAND2    1
    #define FS_SD       2
    #define FS_MS       3
    #define FS_CF       4
    #define FS_XD       5
    #define FS_USBH     6
    #define FS_CDROM_NF 7
    #define FS_DEV_MAX  7
/* DEV MODE */
    #define MSC_EN            0
    #define SD_EN             1
    #define SD_DUAL_SUPPORT   0
    #define NAND1_EN          0
    #define CFC_EN            0
    #define NAND2_EN          0
    #define NAND3_EN          0
    #define USB_NUM           0
    #define NOR_EN            0
    #define XD_EN             0
    #define RAMDISK_EN        0
    #define CDROM_EN          0
    //#define MAX_DISK_NUM     (MSC_EN + SD_EN + CFC_EN + NAND1_EN + NAND2_EN + NAND3_EN + USB_NUM + NOR_EN + RAMDISK_EN)
    #define MAX_DISK_NUM      FS_DEV_MAX


  #endif // _FS_DEFINE_
  #define NVRAM_V1    1
  #define NVRAM_V2    2
  #define NVRAM_V3    3
  #define NVRAM_MANAGER_FORMAT    NVRAM_V3

  #define SUPPORT_STG_SUPER_PLUGOUT     1   // Dominant add super plug 

#define FAT_SPEEDUP_TAB_ENTRY			256///128


#endif 		// __GPLIB_CFG_H__
