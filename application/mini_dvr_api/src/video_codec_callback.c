#include "video_codec_callback.h"
#include "video_codec_osd.h"

#if VIDEO_ENCODE_USE_MODE ==SENSOR_BUF_FIFO_MODE
	INT8U csi_fifo_flag = 0;
#endif
INT8U  Display_Device = DISPLAY_DEVICE;
INT8U  g_display_mode = TV_TFT_MODE;
INT32U video_codec_show_buffer;
INT32U video_codec_display_flag;
void 	(*tft_vblank_callback)(void);
#if C_MOTION_DETECTION == CUSTOM_ON	
	void 	(*motion_detect_callback)(void);
#endif	

#if VIDEO_DISPALY_WITH_PPU
extern PPU_REGISTER_SETS *video_ppu_register_set;
#endif
extern TIME_T	g_current_time;
	
//=======================================================================================
//		Video decode stop hook function 
//=======================================================================================
void video_decode_end(void)
{
	DBG_PRINT("AVI Decode play-end callback...\r\n");
}

//=======================================================================================
//		Video decode output buffer hook function 
//=======================================================================================
void video_decode_FrameReady(INT8U *FrameBufPtr)
{
#if	VIDEO_DISPALY_WITH_PPU == 0
    video_codec_show_buffer = (INT32U)FrameBufPtr;
 	video_codec_display_flag = 1;
#else	//display with PPU
	gplib_ppu_text_calculate_number_array(video_ppu_register_set, C_PPU_TEXT1, 640, 480, (INT232U)FrameBufPtr);	
	result = gplib_ppu_go_and_wait_done(video_ppu_register_set);
#endif
}


//=======================================================================
//  Video encode hook function 
//=======================================================================
//=======================================================================
//		sensor start 
//=======================================================================
INT32U video_encode_sensor_start(INT32U csi_frame1, INT32U csi_frame2)
{
	OS_CPU_SR cpu_sr;
	INT32U temp;
	// Setup CMOS sensor
	//OS_ENTER_CRITICAL();
	OSSchedLock();
	R_TGR_IRQ_STATUS = 0x31;
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	CSI_Init(SENSOR_WIDTH, SENSOR_HEIGHT, FT_CSI_YUVIN|FT_CSI_YUVOUT|FT_CSI_RGB1555, csi_frame1, NULL);
	R_TGR_IRQ_EN = 0x1;		//enable csi frame end irq
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	CSI_Init(SENSOR_WIDTH, SENSOR_HEIGHT, FT_CSI_YUVIN|FT_CSI_YUVOUT|FT_CSI_RGB1555, csi_frame1, csi_frame2);
	R_TGR_IRQ_EN = 0x31;	//enable csi fifo irq and frame end irq and fifo under-run
//+++++++++++++++++++++++++++++++++++++	
	R_TGR_IRQ_STATUS = 0x31;
	OS_ENTER_CRITICAL();
	while(1) {		
		temp = R_TGR_IRQ_STATUS;
		if(temp & 0x0001) {
			R_CSI_TG_CTRL0 &= ~0x0001;	//disable sensor controller
			ASM(NOP);
			ASM(NOP);
			R_CSI_TG_CTRL0 |= 0x0001;
			ASM(NOP);
			ASM(NOP);
			break;
		}
	}
	R_TGR_IRQ_STATUS = 0x31;
	OS_EXIT_CRITICAL();
//+++++++++++++++++++++++++++++++++++++	
#endif	
	//OS_EXIT_CRITICAL();
	OSSchedUnlock();
	vic_irq_enable(VIC_CSI);
	
    return 0;
}

//=======================================================================
//		sensor stop 
//=======================================================================
INT32U video_encode_sensor_stop(void)
{
	INT32U temp;

	*P_CSI_TG_FBSADDR = 0x40000000;
	*P_CSI_TG_FBSADDR_B = 0x40000000;

  	//close sensor
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	R_TGR_IRQ_EN &= ~0x1;
	R_TGR_IRQ_STATUS = 0x1;	
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	R_TGR_IRQ_EN &= ~0x31;
	//R_TGR_IRQ_STATUS = 0x31;
#endif
	//R_CSI_TG_CTRL0 = 0;
	//R_CSI_TG_CTRL1 = 0;	
	R_CSI_TG_CTRL0 &= ~0x0001;	//disable sensor controller
	R_CSI_TG_CTRL1 &= ~0x0080;	//disable sensor clock out
    
    temp = R_TGR_IRQ_STATUS;
    R_TGR_IRQ_STATUS = temp;
    return 0;
}

//=======================================================================
//		preview frame buffer ready  
//=======================================================================
INT32S video_encode_display_frame_ready(INT32U frame_buffer)
{

#if	VIDEO_DISPALY_WITH_PPU == 0           
	video_codec_display_flag = 1;
  	video_codec_show_buffer = frame_buffer;
#else	//display with ppu
	INT32S result;
	frame_buffer = R_PPU_FBO_ADDR;//g_ppu_frame;
	gplib_ppu_text_calculate_number_array(video_ppu_register_set, C_PPU_TEXT1, 640, 480, frame_buffer);	// Calculate Number array
	result = gplib_ppu_go(video_ppu_register_set);
	if(result < 0)
		frame_buffer =  -1;	
#endif

	return frame_buffer;		
}

//=======================================================================
//		encode frame buffer ready when use user define mode 
//=======================================================================
void  video_encode_frame_ready(void *WorkMem, AVIPACKER_FRAME_INFO* pFrameInfo)
{
	/*
	if(fourcc == 0x63643030)
	{	//"00dc", video frame ready 
	
	}
	else if(fourcc == 0x62773130)
	{	//"01wb", audio frame ready
	
	}
	else
		return STATUS_FAIL;
	
	return STATUS_OK;
	*/
}

//=======================================================================
//		avi encode end, 
// ISRC: source; IART: Artist; ICOP: copyright; IARL: achival location
// ICMS: commissioned; ICMT: comments;  ICRD: create date; ICRP: cropped
// IDIM: dimensions; IDPI: dots per inch; IENG: engineer; IGNR: genre
// IKEY: keyword; ILGT: lightness; IMED: medium; INAM: name;
// IPLT: palette setting; IPRD: product; ISFT: software; ISHP: sharpness
// ISRC: source; ISRF: source form; ITCH: technician
//=======================================================================
void video_encode_end(void *workmem)
{
	//add description to avi file
	//int AviPackerV3_AddInfoStr(const char *fourcc, const char *info_string);
	AviPackerV3_AddInfoStr(workmem, "ISRC", "Generplus");
	AviPackerV3_AddInfoStr(workmem, "IART", "Generplus");
	AviPackerV3_AddInfoStr(workmem, "ICOP", "Generplus");
	AviPackerV3_AddInfoStr(workmem, "ICRD", "2010-06-29");	
}

//--------------------------------------------------------------------
//	user can add special effect in audio or video when use video encode
//  
//---------------------------------------------------------------------
#if AUDIO_SFX_HANDLE
extern INT16S   *pcm_bg_out[];
extern OS_EVENT    *aud_bg_send_q;

INT32U video_encode_audio_sfx(INT16U *PCM_Buf, INT32U cbLen)
{
	INT32S i, index;
	INT32U Temp;
	OS_Q_DATA OSQData; 
	AUDIO_ARGUMENT aud_arg;
	
	aud_arg.Main_Channel = 0;
	    
	if(audio_decode_status(aud_arg) == AUDIO_CODEC_PROCESSING)
	{
		OSQQuery(aud_bg_send_q, &OSQData);
		index = OSQData.OSNMsgs-1;
		if(index<0)
			index = 0;
		
		for(i = 0; i<(cbLen/2) ; i++)
		{
			Temp = (INT16U) pcm_bg_out[index][i];
			Temp +=  PCM_Buf[i];
			PCM_Buf[i] = Temp >> 1;
		}
	}
	
	return (INT32U)PCM_Buf;
}
#endif

#if VIDEO_SFX_HANDLE
INT32U video_encode_video_sfx(INT32U buf_addr, INT32U cbLen)
{
	INT32U i;
	INT8U *pdata = (INT8U *)buf_addr;
	
	for(i=0; i<cbLen ;i++)
	{
		if(i % 4 == 1)
		 *(pdata + i) <<= 1;	
	}

	return (INT32U)pdata;
}
#endif

void tft_vblank_isr_register(void (*user_isr)(void))
{
	tft_vblank_callback = user_isr;
}

#if C_MOTION_DETECTION == CUSTOM_ON
void motion_detect_isr_register(void (*user_isr)(void))
{
	motion_detect_callback = user_isr;
}
#endif

//=======================================================================
//		Following are the user-defined code for display on TV or TFT LCM
//=======================================================================
//=======================================================================
//	tft and sensor isr handle
//=======================================================================
void user_defined_video_codec_isr_csi(void)
{
#if C_MOTION_DETECTION == CUSTOM_ON
	if(R_PPU_IRQ_STATUS & R_PPU_IRQ_EN & 0x80) //motion detection
	{
		R_PPU_IRQ_STATUS = 0x80;
		if (motion_detect_callback != 0)
    	{
			(*motion_detect_callback)();
		}
	}
#endif	
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE || VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE 
	{
		INT32U status;
		INT8U sensor_underrun;

		sensor_underrun = 0;
		status = R_TGR_IRQ_STATUS;
		if(status & 0x10) {
			R_TGR_IRQ_STATUS = 0x10;
			sensor_underrun = 1;
			DBG_PRINT("+|");
		}

#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
		if(status & 0x21 & R_TGR_IRQ_EN) {
			csi_fifo_flag ^= 0x1;
		}
#endif

		if(status & 0x01 & R_TGR_IRQ_EN) { 			// frame end
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
			video_encode_auto_switch_csi_frame();
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	   		video_encode_auto_switch_csi_frame_end(csi_fifo_flag);
		} else if(status & 0x20 & R_TGR_IRQ_EN) { 	// fifo end
	   		video_encode_auto_switch_csi_fifo_end(csi_fifo_flag);
#endif
		}
		R_TGR_IRQ_STATUS = status;
	}
#endif
}

void user_defined_video_codec_entrance(void)
{
#if VIDEO_DISPALY_WITH_PPU == 0
	video_codec_display_flag = 0;	
	R_PPU_IRQ_EN = 0;			// disable all ppu interrupt
	R_PPU_IRQ_STATUS = 0x7FFF;	// Clear all PPU interrupts 	
		vic_irq_register(VIC_CSI, user_defined_video_codec_isr_csi);
	/*
	if(DISPLAY_DEVICE == DISPLAY_TFT)	
		R_PPU_IRQ_EN = 0x2000; // TFT Vertical-Blanking IRQ enable register
	else
		R_PPU_IRQ_EN = 0x0800;
	*/
//	vic_irq_enable(VIC_PPU);
#else
	video_codec_ppu_configure();
#endif
}

void video_codec_show_image(INT8U TV_TFT, INT32U BUF,INT32U DISPLAY_MODE ,INT32U SHOW_TYPE)
{
    switch(SHOW_TYPE)
	{      
	   case IMAGE_OUTPUT_FORMAT_RGB565:
	        if(DISPLAY_MODE==QVGA_MODE)
	           R_PPU_ENABLE=(PPU_QVGA_MODE|PPU_FRAME_BASE_MODE|PPU_RGB565_MODE);
	        else if(DISPLAY_MODE == VGA_MODE)
	           R_PPU_ENABLE=(PPU_VGA_MODE|PPU_FRAME_BASE_MODE|PPU_RGB565_MODE);   
	        else if(DISPLAY_MODE == D1_MODE)
	           	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_720x480|PPU_FRAME_BASE_MODE|PPU_RGB565_MODE);
	        else if(DISPLAY_MODE == TFT_320x240_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_320X240|PPU_FRAME_BASE_MODE|PPU_RGB565_MODE);
	   		else if(DISPLAY_MODE == TFT_800x480_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_800X480|PPU_FRAME_BASE_MODE|PPU_RGB565_MODE);	    
	   		break;
	   
	   case IMAGE_OUTPUT_FORMAT_RGBG:
	        if(DISPLAY_MODE==QVGA_MODE)
	           R_PPU_ENABLE=(PPU_QVGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE2);
	        else if(DISPLAY_MODE == VGA_MODE)
	           R_PPU_ENABLE=(PPU_VGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE2);
	        else if(DISPLAY_MODE == D1_MODE)
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_720x480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE2);
	        else if(DISPLAY_MODE == TFT_320x240_MODE) 
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_320X240|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE2);
	        else if(DISPLAY_MODE == TFT_800x480_MODE)
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_800X480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE2);
	   		break;
	   
	   case IMAGE_OUTPUT_FORMAT_GRGB:
	   		if(DISPLAY_MODE==QVGA_MODE)
	           	R_PPU_ENABLE=(PPU_QVGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE3);
	        else if(DISPLAY_MODE == VGA_MODE)
	           	R_PPU_ENABLE=(PPU_VGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE3);
	        else if(DISPLAY_MODE == D1_MODE)
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_720x480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE3);
	        else if(DISPLAY_MODE == TFT_320x240_MODE) 
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_320X240|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE3);
	        else if(DISPLAY_MODE == TFT_800x480_MODE)
	        	R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_800X480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_RGBG_MODE|PPU_RGBG_TYPE3);
	   		break;
	  
	   case IMAGE_OUTPUT_FORMAT_UYVY:
	        if(DISPLAY_MODE == QVGA_MODE)
	           R_PPU_ENABLE=(PPU_QVGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE2);
	        else if(DISPLAY_MODE == VGA_MODE)
	           R_PPU_ENABLE=(PPU_VGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE2);    
	   		else if(DISPLAY_MODE == D1_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_720x480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE2);
	   		else if(DISPLAY_MODE == TFT_320x240_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_320X240|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE2);
	   		else if(DISPLAY_MODE == TFT_800x480_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_800X480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE2);	
	   		break;
	
	   case IMAGE_OUTPUT_FORMAT_YUYV:
	        if(DISPLAY_MODE == QVGA_MODE)
	           R_PPU_ENABLE=(PPU_QVGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE3);
	        else if(DISPLAY_MODE == VGA_MODE)
	           R_PPU_ENABLE=(PPU_VGA_MODE|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE3);    
	   		else if(DISPLAY_MODE == D1_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_720x480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE3);
	   		else if(DISPLAY_MODE == TFT_320x240_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_320X240|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE3);
	   		else if(DISPLAY_MODE == TFT_800x480_MODE)
	   			R_PPU_ENABLE=(PPU_QVGA_MODE|TFT_SIZE_800X480|PPU_FRAME_BASE_MODE|PPU_YUYV_RGBG_FORMAT_MODE|PPU_YUYV_MODE|PPU_YUYV_TYPE3);
	   		break;
	} 

	if (TV_TFT == 0)//TV
		R_TV_FBI_ADDR = BUF;
	else		//TFT
		R_TFT_FBI_ADDR = BUF;
}
