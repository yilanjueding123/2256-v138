#ifndef AVI_ENCODER_SCALER_JPEG_H_
#define AVI_ENCODER_SCALER_JPEG_H_

//=====================================================================================================
/* avi encode configure set */

//video 
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE 
	#define AVI_ENCODE_DIGITAL_ZOOM_EN		0	//0: disable digital zoom in/out 1: enable digital zoom in/out 
	#define AVI_ENCODE_PREVIEW_DISPLAY_EN	C_VIDEO_PREVIEW	//0: disable display preview	1: enable display preview
	#define AVI_ENCODE_PRE_ENCODE_EN		0	//0: disable, 1: audio and video pre-encode enable 
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	#define AVI_ENCODE_DIGITAL_ZOOM_EN		0	//fix to 0
	#define AVI_ENCODE_PREVIEW_DISPLAY_EN	C_VIDEO_PREVIEW	//fix to 0
	#define AVI_ENCODE_PRE_ENCODE_EN		0 	//fix to 0
#endif
#define AVI_ENCODE_VIDEO_ENCODE_EN		1		//0: disable, 1:enable; video record enable
#define AVI_ENCODE_FAST_SWITCH_EN		0//C_CYCLIC_VIDEO_RECORD		//0: disable, 1:enable; video encode fast stop and start 
#define AVI_ENCODE_VIDEO_TIMER			TIMER_B //timer, A,B,C
#define AVI_ENCODE_TIME_BASE			((AVI_FRAME_RATE*5)/3)

//buffer number 
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE 
	#define AVI_ENCODE_CSI_BUFFER_NO		1	//sensor use buffer number
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	#define AVI_ENCODE_CSI_BUFFER_NO		5//12  	//fix to 2 
#endif
#define AVI_ENCODE_SCALER_BUFFER_NO		2	//scaler use buffer number
#define AVI_ENCODE_DISPALY_BUFFER_NO	2	//display use buffer number
#define AVI_ENCODE_VIDEO_BUFFER_NO		8//10//2	//jpeg encode use buffer number
#define AVI_ENCODE_PCM_BUFFER_NO		3	//audio record pcm use buffer number

#define AVI_ENCODE_VIDEO_BUFFER_SIZE	(160*1024)	//(120*1024) //jpeg encode use buffer size
//audio format	
#define AVI_ENCODE_AUDIO_FORMAT			WAVE_FORMAT_PCM //0: no audio, WAVE_FORMAT_PCM, WAVE_FORMAT_IMA_ADPCM and WAVE_FORMAT_ADPCM 
//audio encode buffer size
#define C_WAVE_ENCODE_TIMES				15	//audio encode times

#define AVI_AUDIO_RECORD_TIMER		ADC_AS_TIMER_F  //adc use timer, C ~ F
#define AVI_AUDIO_RECORD_ADC_CH		ADC_LINE_1		//adc use channel, 0 ~ 3

//avi file max size
#define AVI_ENCODE_CAL_DISK_SIZE_EN		0				//0: disable, 1: enable	

//Motion Dection
#define MD_RAM_SIZE             1200
//sdram address of non-cache
#define SDRAM_BASE_ADDR         0x80000000
#define C_MD_8X8_MODE		0
#define C_MD_16X16_MODE		1
#define C_MD_QVGA_MODE		0
#define C_MD_VGA_MODE		1

//mpeg4
#define MPEG4_ENCODE_ENABLE				0				//1: enable, 0: disable
#define C_MP4_INPUT_VY1UY0				0x00000
#define C_MP4_INPUT_Y1UY0V				0x00001
#define C_MP4_INPUT_Y1VY0U				0x00002
#define C_MP4_INPUT_UY1VY0				0x00003
#define MP4_ENC_INPUT_FORMAT			C_MP4_INPUT_Y1UY0V

//video encode fifo mode, fifo line set
#define SENSOR_FIFO_8_LINE				(1<<3)
#define SENSOR_FIFO_16_LINE				(1<<4)
#define SENSOR_FIFO_32_LINE				(1<<5)
#define SENSOR_FIFO_LINE				SENSOR_FIFO_16_LINE//SENSOR_FIFO_16_LINE

//video format
#define C_XVID_FORMAT					0x44495658
#define	C_MJPG_FORMAT					0x47504A4D
#define AVI_ENCODE_VIDEO_FORMAT			C_MJPG_FORMAT //only support mjpeg 
//=====================================================================================================
//=====================================================================================================
/* avi encode status */
#define C_AVI_ENCODE_IDLE       0x00000000
#define C_AVI_ENCODE_PACKER0	0x00000001
#define C_AVI_ENCODE_AUDIO      0x00000002
#define C_AVI_ENCODE_VIDEO      0x00000004
#define C_AVI_ENCODE_SCALER     0x00000008
#define C_AVI_ENCODE_SENSOR     0x00000010
#define C_AVI_ENCODE_PACKER1	0x00000020
#define C_AVI_ENCODE_MD			0x00000040
#define C_AVI_ENCODE_USB_WEBCAM 0x00000080

#define C_AVI_ENCODE_START      0x00000100
#define C_AVI_ENCODE_PAUSE		0x00000200
#define C_AVI_VID_ENC_START		0x00000400	
#define C_AVI_ENCODE_FRAME_END	0x40000000
#define C_AVI_ENCODE_FIFO_ERR	0x80000000

//gpy0500 command
#define C_CMD_RESET_IN0				0x81
#define C_CMD_RESET_IN1				0x83
#define C_CMD_RESET_IN2				0x85
#define C_CMD_RESET_IN3				0x87
#define C_CMD_RESET_IN4				0x89		
#define C_CMD_ENABLE_ADC			0x98
#define C_CMD_ENABLE_MIC_AGC_ADC	0x9B
#define C_CMD_ENABLE_MIC_ADC		0x99
#define C_CMD_ENABLE_MIC_AGC		0x93
#define C_CMD_DUMMY_COM			0xC0
#define C_CMD_ADC_IN0				0x80
#define C_CMD_ADC_IN1				0x82
#define C_CMD_ADC_IN2				0x84
#define C_CMD_ADC_IN3				0x86
#define C_CMD_ADC_IN4				0x88
#define C_CMD_ZERO_COM				0x00
#define C_CMD_POWER_DOWN			0x90
#define C_CMD_TEST_MODE				0xF0

//other
#define C_ACK_SUCCESS	0x00000001
#define C_ACK_FAIL		0x80000000

//scaler
#define C_SCALER_FULL_SCREEN		0
#define C_SCALER_FIT_RATIO		  	1
#define C_NO_SCALER_FIT_BUFFER  	2
#define C_SCALER_FIT_BUFFER			3
#define C_SCALER_ZOOM_FIT_BUFFER	4

#ifndef DBG_MESSAGE
	#define DBG_MESSAGE 0
#endif

#if DBG_MESSAGE
	#define DEBUG_MSG(x)	{x;}
#else
	#define DEBUG_MSG(x)	{}
#endif

#define RETURN(x)	{nRet = x; goto Return;}
#define POST_MESSAGE(msg_queue, message, ack_mbox, msc_time, msg, err)\
{\
	msg = (INT32S) OSMboxAccept(ack_mbox);\
	err = OSQPost(msg_queue, (void *)message);\
	if(err != OS_NO_ERR)\
	{\
		DEBUG_MSG(DBG_PRINT("OSQPost Fail!!!\r\n"));\
		RETURN(STATUS_FAIL);\
	}\
	msg = (INT32S) OSMboxPend(ack_mbox, msc_time/10, &err);\
	if(err != OS_NO_ERR || msg == C_ACK_FAIL)\
	{\
		DEBUG_MSG(DBG_PRINT("OSMbox ack Fail!!!\r\n"));\
		RETURN(STATUS_FAIL);\
	}\
}

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
//-------------------------------------------------------------------------
// For capturing photo with interval

typedef struct jpeg_compress_buf_info_s
{
	INT32U jpeg_buf_addrs;
	INT32U jpeg_vlc_size;
}jpeg_compress_buf_t;

#define PHOTO_COMPRESS_CNT	(SENSOR_HEIGHT/SENSOR_FIFO_LINE) //30

typedef struct jpeg_comress_args_s
{
	INT16S jpeg_file_handle;
	INT8U  jpeg_img_format;
	INT32U jpeg_img_width;
	INT32U jpeg_img_height;
	INT32U jpeg_input_addrs;
	INT32U jpeg_input_size;
	INT32U jpeg_compress_buf_idx;
	INT8U  jpeg_compress_buf_max;	
	jpeg_compress_buf_t jpeg_compress_buf[PHOTO_COMPRESS_CNT];
}capture_photo_args_t;

//-------------------------------------------------------------------------
#endif

#endif //AVI_ENCODER_SCALER_JPEG_H_
