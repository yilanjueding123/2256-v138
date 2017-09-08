#include "ap_startup.h"
#include "stdio.h"

#include "drv_l1_tft.h"
#include "drv_l2_spi_flash.h"
#include "avi_encoder_app.h"
#include "application.h"

#include "math.h"
#include "string.h"
static INT32U STRING_Convert_HEX(CHAR *str)
{
    INT32U sum = 0;
    INT8U len;
    INT16S i;

    len = strlen(str);
    for(i = 0; i<len; i++)
    {
        if (str[i]>='0'&&str[i]<='9') sum += (str[i]-'0')*(pow(16,len-1-i));
        else if(str[i]>='a'&&str[i]<='f') sum += (str[i]-'a'+10)*(pow(16,len-1-i));
        else if(str[i]>='A'&&str[i]<='F') sum += (str[i]-'A'+10)*(pow(16,len-1-i));
    }
    return sum; 
}

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
extern INT8U csi_fifo_flag; 
void capture_switch(void)
{
	VIDEO_ARGUMENT arg;
	
	video_encode_preview_stop();

	OSTimeDly(10);
	csi_fifo_flag = 1;

	arg.bScaler = 1; 
	arg.TargetWidth = AVI_WIDTH; 
	arg.TargetHeight = AVI_HEIGHT; 

  #if SENSOR_WIDTH==640 && AVI_WIDTH==720                // VGA(640x480) to D1(720x480) engine is used 
	arg.SensorHeight = 432; 
	arg.SensorWidth  = AVI_WIDTH; 
  #else 
	arg.SensorHeight = SENSOR_HEIGHT; 
	arg.SensorWidth  = SENSOR_WIDTH; 
  #endif 

	arg.DisplayWidth = TFT_WIDTH;
	arg.DisplayHeight = TFT_HEIGHT;
	arg.DisplayBufferWidth = TFT_WIDTH;
	arg.DisplayBufferHeight = TFT_HEIGHT;	
	arg.VidFrameRate = AVI_FRAME_RATE;
	arg.AudSampleRate = AVI_AUD_SAMPLING_RATE;
	arg.OutputFormat = IMAGE_OUTPUT_FORMAT_RGB565; 

	video_encode_preview_start(arg);
    curr_Q_value = AVI_Q_VALUE;
}
#endif

extern INT32S AviPacker_mem_alloc(AviEncPacker_t *pAviEncPacker);
void ap_startup_init(void)
{
	VIDEO_ARGUMENT arg;

	arg.bScaler = 1; 
	arg.TargetWidth = AVI_WIDTH; 
	arg.TargetHeight = AVI_HEIGHT; 

  #if SENSOR_WIDTH==640 && AVI_WIDTH==720                // VGA(640x480) to D1(720x480) engine is used 
	arg.SensorHeight = 432; 
	arg.SensorWidth  = AVI_WIDTH; 
  #else 
	arg.SensorHeight = SENSOR_HEIGHT; 
	arg.SensorWidth  = SENSOR_WIDTH; 
  #endif 

	arg.DisplayWidth = TFT_WIDTH;
	arg.DisplayHeight = TFT_HEIGHT;
	arg.DisplayBufferWidth = TFT_WIDTH;
	arg.DisplayBufferHeight = TFT_HEIGHT;	
	arg.VidFrameRate = AVI_FRAME_RATE;
	arg.AudSampleRate = AVI_AUD_SAMPLING_RATE;
	arg.OutputFormat = IMAGE_OUTPUT_FORMAT_RGB565; 

	user_defined_video_codec_entrance();
	video_encode_entrance();
	AviPacker_mem_alloc(pAviEncPacker0);
	video_encode_preview_start(arg);
    avi_enc_packer_init(pAviEncPacker0);
    curr_Q_value = AVI_Q_VALUE;
}


#define C_UPGRADE_BUFFER_SIZE		0x80000//0x80000			// Must be multiple of 64K and must be larger than size of (bootheader + bootloader)
#define C_UPGRADE_SPI_BLOCK_SIZE	0x10000			// 64K
#define C_UPGRADE_SPI_WRITE_SIZE	0x100			// 256 bytes
#define C_UPGRADE_FAIL_RETRY		20				// Must larger than 0

//static INT32U firmware_buffer[C_UPGRADE_BUFFER_SIZE];

static void Send_Upfail_Message(void)
{
	INT32U type;
	//DBG_PRINT("up_fail\r\n");
    type = LED_UPDATE_FAIL; //LED_WAITING_AUDIO_RECORD;
    msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
}

void ap_umi_firmware_upgrade(void)
{
	INT32U /*i,*/ j, k,total_size, complete_size, type;
	INT32U *firmware_buffer;
	struct stat_t statetest;
	INT16S fd;
	INT16S nRet=0;
	INT32S verify_size;
	char   p[5];
	INT8U  FileName_Path[3+5+8+4+1+4];
	INT8U  checksum_temp[8+1];
	INT32U *checksum_buffer; 
	INT32U checksum_addr = 0;
	INT8U  *q;
    INT16U checksum_cnt;   
	INT32U CheckSum = 0; 
	INT32U CheckSum_BIN;  	
	struct f_info	file_info;
	
	
	
	#if 1
	if (storage_sd_upgrade_file_flag_get() != 2) {
		return;
	}
	   //chdir("C:\\");
	
	   sprintf((char*)p,(const char*)"%04d",PRODUCT_NUM);
	  
	   gp_strcpy((INT8S*)FileName_Path,(INT8S*)"C:\\JH_");
	 
	   gp_strcat((INT8S*)FileName_Path,(INT8S*)p);
	   gp_strcat((INT8S*)FileName_Path,(INT8S*)"*.bin");
	   nRet = _findfirst((CHAR*)FileName_Path, &file_info ,D_ALL); 
	  if(nRet < 0) 
	 
	   {
	      // DBG_PRINT("not find file\r\n");
		  return ;
	   }
	  

	  strncpy((CHAR*)checksum_temp,(CHAR*)file_info.f_name+8,8);    
	  
      checksum_temp[8] = NULL;                         
      gp_strcpy((INT8S*)FileName_Path,(INT8S*)"C:\\");
      gp_strcat((INT8S*)FileName_Path,(INT8S*)file_info.f_name);
	#endif

	  fd = open((CHAR*)FileName_Path, O_RDONLY);
	
	  if (fd < 0)
	  	{
             Send_Upfail_Message();
			 return  ;
	    }
	  
	  if (fstat(fd, &statetest)) 
		{
		     close(fd);
			 Send_Upfail_Message();
		     return;
	    }
    OSTaskDel(STORAGE_SERVICE_PRIORITY);
    OSTaskDel(USB_DEVICE_PRIORITY);
	total_size = statetest.st_size;
	verify_size = (INT32S)total_size;
	checksum_buffer = (INT32U *) gp_malloc(256);
	if (!checksum_buffer)
		{
			gp_free((void*)checksum_buffer);
			Send_Upfail_Message();
			return ;
		}
	//DBG_PRINT("stp3\r\n");

	for(k=0; k<total_size/256; k++)
	   {
		   lseek(fd, checksum_addr, SEEK_SET);	 
		   if (read(fd,(INT32U)checksum_buffer,256)<=0)
		   {
			   gp_free((void*)checksum_buffer);
			   Send_Upfail_Message();
			   return ;
		   }
		   q = (INT8U*)checksum_buffer;
		   for(checksum_cnt=0; checksum_cnt<256; checksum_cnt++)
		   {
			   CheckSum += *q++;
		   }
		   checksum_addr += 256;
	   }
	if((total_size%256)!= 0)
		{
	      lseek(fd, checksum_addr, SEEK_SET);
	     // read(fd,(INT32U)checksum_buffer,total_size%256);
		  if (read(fd,(INT32U)checksum_buffer,total_size%256)<=0)
		   {
			   gp_free((void*)checksum_buffer);
			   Send_Upfail_Message();
			   return ;
		   }
	      q = (INT8U*)checksum_buffer;
	      for(checksum_cnt=0; checksum_cnt<(total_size%256); checksum_cnt++)
	          {
				   CheckSum += *q++;
	          }
		}
	 
     //DBG_PRINT("Check_sum=%d\r\n",CheckSum);
	 gp_free((void*)checksum_buffer);                               
     CheckSum_BIN = STRING_Convert_HEX((CHAR*)checksum_temp);       
     DBG_PRINT("Checksum_bin=%d\r\n",CheckSum_BIN);
    if (CheckSum_BIN == CheckSum)  
    {
       type = LED_UPDATE_PROGRAM;
	   msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
       //DBG_PRINT("Checksum ok\r\n");
    }
    else 
    {
		Send_Upfail_Message();
      // DBG_PRINT("Checksum fail\r\n");
       return ;
    }
	OSTimeDly(OS_TICKS_PER_SEC);
	firmware_buffer = (INT32U *) gp_malloc(C_UPGRADE_SPI_BLOCK_SIZE);
	if (!firmware_buffer) {
		close(fd);
		Send_Upfail_Message();
		return;
		}	
	

	complete_size = 0;
	SPI_Flash_init();
	OSTimeDly(1);
	while (complete_size < (total_size /*+ app_sec_id*/)) {
		INT32U buffer_left;
		INT8U retry=0;
		buffer_left = (total_size - complete_size + (C_UPGRADE_SPI_BLOCK_SIZE-1)) & ~(C_UPGRADE_SPI_BLOCK_SIZE-1);	//SPI可写的最大值
		//buffer_left = (total_size + app_sec_id - complete_size + (C_UPGRADE_SPI_BLOCK_SIZE-1)) & ~(C_UPGRADE_SPI_BLOCK_SIZE-1);
		if (buffer_left > C_UPGRADE_BUFFER_SIZE) {
			buffer_left = C_UPGRADE_BUFFER_SIZE;
		}
		while (buffer_left && retry<C_UPGRADE_FAIL_RETRY) {
		//	DBG_PRINT("complete_size=%x\r\n",complete_size);
			complete_size &= ~(C_UPGRADE_SPI_BLOCK_SIZE-1);
			 lseek(fd, complete_size, SEEK_SET);   //设置分段读取TF卡内固件的地址
            if (read(fd,(INT32U)firmware_buffer,C_UPGRADE_SPI_BLOCK_SIZE)<=0)
            {
                close(fd);
			   	gp_free((void *) firmware_buffer);
				return;
    		}
			if(SPI_Flash_erase_block(complete_size)) 
			{
				retry++;
				//DBG_PRINT("erase_fail\r\n");
				continue;
			}
			for (j=C_UPGRADE_SPI_BLOCK_SIZE; j; j-=C_UPGRADE_SPI_WRITE_SIZE) 
			{
				if (SPI_Flash_write_page(complete_size, (INT8U *) (firmware_buffer + ((complete_size & (C_UPGRADE_SPI_BLOCK_SIZE-1))>>2))))
					{
					  break;
				    }
				complete_size += C_UPGRADE_SPI_WRITE_SIZE;
				if(complete_size >= (total_size /*+ app_sec_id*/))
				    {
					 break;
				    }
			}

			if(complete_size >= (total_size /*+ app_sec_id*/))
			{
				break;
			}

			if (j == 0) 
			{
				buffer_left -= C_UPGRADE_SPI_BLOCK_SIZE;
			} else 
			{
				retry++;
			}
		}

	
	}

	close(fd);
	gp_free((void *) firmware_buffer);
	//unlink("C:\\gp_firmware_upgrade.bin");
	//gpio_init_io(IO_F7, GPIO_INPUT);
	//gpio_set_port_attribute(IO_F7, ATTRIBUTE_HIGH);
	//R_ADC_USELINEIN = 0xB;	//for mini DVR enable 4 of AD pad
	R_ADC_USELINEIN = 0;	//for mini DVR enable 4 of AD pad
	msgQFlush(ApQ);
	type = LED_UPDATE_FINISH;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
	OSTimeDly(OS_TICKS_PER_SEC*5);
	//ap_state_handling_power_off_handle(NULL);
}





