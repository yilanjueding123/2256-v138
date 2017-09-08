#include "avi_encoder_app.h"
#include "font.h"
#include "avi_encoder_state.h"

#if MPEG4_ENCODE_ENABLE == 1
#include "drv_l1_mpeg4.h"
#endif

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
//-------------------------------------------------------------------------
// For capturing photo with interval
extern INT8U jpeg_picture_header[624];

capture_photo_args_t capture_photo_args = {0};

//-------------------------------------------------------------------------
#endif

/* os task stack size */
#define C_SCALER_STACK_SIZE			128
#define	C_JPEG_STACK_SIZE			128
#define C_SCALER_QUEUE_MAX			32
#define C_JPEG_QUEUE_MAX			32
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	#define C_CMOS_FRAME_QUEUE_MAX	AVI_ENCODE_CSI_BUFFER_NO
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	#define C_CMOS_FRAME_QUEUE_MAX	1024/SENSOR_FIFO_LINE	//for sensor height = 1024
#endif

const INT8U *number[] = {	
	acFontHZArial01700030, 
	acFontHZArial01700031, 
	acFontHZArial01700032,
	acFontHZArial01700033, 
	acFontHZArial01700034,
	acFontHZArial01700035,
	acFontHZArial01700036,
	acFontHZArial01700037, 
	acFontHZArial01700038,
	acFontHZArial01700039 
};

/* os tsak stack */
INT32U	ScalerTaskStack[C_SCALER_STACK_SIZE];
INT32U	JpegTaskStack[C_JPEG_STACK_SIZE];

/* os task queue */
OS_EVENT *scaler_task_q; 
OS_EVENT *scaler_task_ack_m;
OS_EVENT *cmos_frame_q;
OS_EVENT *vid_enc_task_q;
OS_EVENT *vid_enc_task_ack_m;
OS_EVENT *scaler_hw_sem;
void *scaler_task_q_stack[C_SCALER_QUEUE_MAX];
void *cmos_frame_q_stack[C_CMOS_FRAME_QUEUE_MAX];
void *video_encode_task_q_stack[C_JPEG_QUEUE_MAX];

#if AVI_ENCODE_PREVIEW_DISPLAY_EN == 1
	INT32U video_preview_flag;
#endif


//=======================================================================
//  Dynamic tune Q function
//=======================================================================
INT32S curr_Q_value = AVI_Q_VALUE;

void dynamic_tune_jpeg_Q(INT32U vlcSize)
{
	if(vlcSize > (AVI_ENCODE_VIDEO_BUFFER_SIZE - 10*1024))			//大于150KB, q=20
	{
		curr_Q_value -= 30;
	}
	else if (vlcSize > AVI_ENCODE_VIDEO_BUFFER_SIZE - (30*1024))	//大于130KB, q -= 5;
	{
		curr_Q_value -= 5;
	}
	else if (vlcSize < AVI_ENCODE_VIDEO_BUFFER_SIZE - (60*1024))	//小于100KB, q += 5;
	{																//在 101KB - 129KB 之间, q 不变化.
		curr_Q_value += 5;
	}
	if (curr_Q_value < 20) curr_Q_value = 20;
	if (curr_Q_value > AVI_Q_VALUE) curr_Q_value = AVI_Q_VALUE;	

	//DBG_PRINT("\r\nQ[%d]", curr_Q_value);
}

//=======================================================================
//  Draw OSD function
//=======================================================================
void cpu_draw_osd(const INT8U *source_addr, INT32U target_addr, INT16U offset, INT16U res){	
	const INT8U* chptr;
	INT8U i,j,tmp;
	INT8U *ptr;
	
	ptr = (INT8U*) target_addr;
	ptr+= offset;
	chptr = source_addr;
	for(i=0;i<19;i++){
		tmp = *chptr++;
		for(j=0;j<8;j++){
			if(tmp&0x80)
			{	
				*ptr++ =0x80;
				*ptr++ = 0xff;
			}
			else
			{
				ptr += 2;
			}	
			tmp = tmp<<1;
		}
 	//ptr += (320-8)*2;
 	ptr += (res-8)*2;
 	}
} 
//=======================================================================
//  Draw OSD function
//=======================================================================
void cpu_draw_time_osd(TIME_T current_time, INT32U target_buffer, INT16U resolution)
{
	INT8U  data;
	INT16U offset, space, wtemp;
	INT32U line;

  #if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	if (resolution == 640) {
		line = target_buffer + 400*resolution*2;
		offset = 430*2;
	} else if (resolution == 720) {
		line = target_buffer + 400*resolution*2;
		offset = 480*2;
	} else if (resolution == 1280) {
		line = target_buffer + 700*resolution*2;
		offset = 1000*2;
	} else {
		return;
	}
  #else
	if (resolution == 640) {
		line = target_buffer + 8*resolution*2;
		offset = 430*2;
	} else if (resolution == 720) {
		line = target_buffer + 8*resolution*2;
		offset = 480*2;
	} else if (resolution == 1280) {
		line = target_buffer + 8*resolution*2;
		offset = 1000*2;
	} else {
		return;
	}
  #endif

	space = 16;
//Arial 17
	// year
	if (current_time.tm_year > 2008) {
		wtemp = current_time.tm_year - 2000;
		cpu_draw_osd(number[2], line, offset, resolution);
		cpu_draw_osd(number[0],line,offset+space*1,resolution);
		data = wtemp/10;
		wtemp -= data*10;
		cpu_draw_osd(number[data],line,offset+space*2,resolution);
		data = wtemp;
		cpu_draw_osd(number[data],line,offset+space*3,resolution);
	} else {
		cpu_draw_osd(number[2], line, offset, resolution);
		cpu_draw_osd(number[0],line,offset+space*1,resolution);
		cpu_draw_osd(number[0],line,offset+space*2,resolution);
		cpu_draw_osd(number[8],line,offset+space*3,resolution);
	}
	
	// :
	cpu_draw_osd(acFontHZArial017Slash,line,offset+space*4,resolution);
	
	//month
	wtemp = current_time.tm_mon; 
	cpu_draw_osd(number[wtemp/10],line,offset+space*5,resolution);
	cpu_draw_osd(number[wtemp%10],line,offset+space*6,resolution);
	
	//:
	cpu_draw_osd(acFontHZArial017Slash,line,offset+space*7,resolution);
	
	//day
	wtemp = current_time.tm_mday;
	cpu_draw_osd(number[wtemp/10],line,offset+space*8,resolution);
	cpu_draw_osd(number[wtemp%10],line,offset+space*9,resolution);
	
	//hour
	wtemp = current_time.tm_hour;
	cpu_draw_osd(number[wtemp/10],line,offset+space*11,resolution);
	cpu_draw_osd(number[wtemp%10],line,offset+space*12,resolution);
	
	// :
	cpu_draw_osd(acFontHZArial017Comma, line,offset+space*13,resolution);
	
	//minute
	wtemp = current_time.tm_min;
	cpu_draw_osd(number[wtemp/10],line,offset+space*14,resolution);
	cpu_draw_osd(number[wtemp%10],line,offset+space*15,resolution);
	
	// :
	cpu_draw_osd(acFontHZArial017Comma,line,offset+space*16,resolution);
	
	//second
	wtemp = current_time.tm_sec;
	cpu_draw_osd(number[wtemp/10], line,offset+space*17,resolution);
	cpu_draw_osd(number[wtemp%10],line,offset+space*18,resolution);	
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// scaler task
///////////////////////////////////////////////////////////////////////////////////////////////////////
INT32S scaler_task_create(INT8U pori)
{
	INT8U  err;
	INT32S nRet;
	
	scaler_task_q = OSQCreate(scaler_task_q_stack, C_SCALER_QUEUE_MAX);
	if(!scaler_task_q) RETURN(STATUS_FAIL);
	
	scaler_task_ack_m = OSMboxCreate(NULL);
	if(!scaler_task_ack_m) RETURN(STATUS_FAIL);
	
	cmos_frame_q = OSQCreate(cmos_frame_q_stack, C_CMOS_FRAME_QUEUE_MAX);
	if(!cmos_frame_q) RETURN(STATUS_FAIL);	
	
	scaler_hw_sem = OSSemCreate(1);
	if(!scaler_hw_sem) RETURN(STATUS_FAIL);
	
	err = OSTaskCreate(scaler_task_entry, (void *)NULL, &ScalerTaskStack[C_SCALER_STACK_SIZE - 1], pori);	
	if(err != OS_NO_ERR) RETURN(STATUS_FAIL);
	
	nRet = STATUS_OK;
Return:
	return nRet;
}

INT32S scaler_task_del(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(scaler_task_q, MSG_SCALER_TASK_EXIT, scaler_task_ack_m, 5000, msg, err);
Return:	
	OSQFlush(scaler_task_q);
   	OSQDel(scaler_task_q, OS_DEL_ALWAYS, &err);
   	
   	OSQFlush(cmos_frame_q);
   	OSQDel(cmos_frame_q, OS_DEL_ALWAYS, &err);
   	
	OSMboxDel(scaler_task_ack_m, OS_DEL_ALWAYS, &err);
	OSSemDel(scaler_hw_sem, OS_DEL_ALWAYS, &err);
	return nRet;
}

INT32S scaler_task_start(void)
{
	INT8U  err;
	INT32S nRet, msg;

	if(avi_encode_memory_alloc() < 0)
	{
		avi_encode_memory_free();
		DEBUG_MSG(DBG_PRINT("avi memory alloc fail!!!\r\n"));
		RETURN(STATUS_FAIL);				
	}

	nRet = STATUS_OK;
	POST_MESSAGE(scaler_task_q, MSG_SCALER_TASK_INIT, scaler_task_ack_m, 5000, msg, err);
Return:
	return nRet;	
}

INT32S scaler_task_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(scaler_task_q, MSG_SCALER_TASK_STOP, scaler_task_ack_m, 5000, msg, err);
Return:		
	avi_encode_memory_free();
	return nRet;
}

void scaler_task_entry(void *parm)
{
	INT8U err, scaler_mode, skip_cnt;
	INT16U csi_width, csi_height;
	INT16U encode_width, encode_height; 
	INT16U dip_width, dip_height;
	INT16U dip_buff_width, dip_buff_height;
	INT32U msg_id, sensor_frame, scaler_frame, display_frame;
	INT32U input_format, output_format, display_input_format, display_output_format;

	while(1)
	{
		msg_id = (INT32U) OSQPend(scaler_task_q, 0, &err);
		if((err != OS_NO_ERR)||	!msg_id)
			continue;
			
		switch(msg_id & 0xFF000000)
		{
		case MSG_SCALER_TASK_INIT:
			skip_cnt = 0;
			csi_width = pAviEncVidPara->sensor_capture_width;
			csi_height = pAviEncVidPara->sensor_capture_height;
			encode_width = pAviEncVidPara->encode_width;
			encode_height = pAviEncVidPara->encode_height;
			dip_width = pAviEncVidPara->display_width;
			dip_height = pAviEncVidPara->display_height;
			dip_buff_width = pAviEncVidPara->display_buffer_width;
			dip_buff_height = pAviEncVidPara->display_buffer_height;
			input_format = pAviEncVidPara->sensor_output_format;
			output_format = C_SCALER_CTRL_OUT_YUYV;
			display_frame = 0;
			pAviEncPara->fifo_enc_err_flag = 0;
			pAviEncPara->fifo_irq_cnt = 0; 
			pAviEncPara->vid_pend_cnt = 0;
			pAviEncPara->vid_post_cnt = 0;
			display_input_format = C_SCALER_CTRL_IN_YUYV;
			display_output_format = pAviEncVidPara->display_output_format;
			if(pAviEncVidPara->scaler_flag)
				scaler_mode = C_SCALER_FIT_BUFFER;
				//scaler_mode = C_NO_SCALER_FIT_BUFFER;
			else
				scaler_mode = C_SCALER_FULL_SCREEN;
				
			OSMboxPost(scaler_task_ack_m, (void*)C_ACK_SUCCESS);
			break;
		
		case MSG_SCALER_TASK_STOP:
			OSQFlush(scaler_task_q);
			OSMboxPost(scaler_task_ack_m, (void*)C_ACK_SUCCESS);
			break;
				
		case MSG_SCALER_TASK_EXIT:
			OSMboxPost(scaler_task_ack_m, (void*)C_ACK_SUCCESS);
			OSTaskDel(OS_PRIO_SELF);
			break;
#if AVI_ENCODE_PREVIEW_DISPLAY_EN == 1		
		case MSG_SCALER_TASK_PREVIEW_ON:
			video_preview_flag = 1;
			break;
		
		case MSG_SCALER_TASK_PREVIEW_OFF:
			video_preview_flag = 0;
			break;
		
		case MSG_SCALER_TASK_PREVIEW_UNLOCK:
			{
				INT32U ack_ptr, tmp_ptr, i;
				ack_ptr = (msg_id & 0xFFFFFF);
				for (i=0;i<AVI_ENCODE_DISPALY_BUFFER_NO;i++) {
					tmp_ptr = (pAviEncVidPara->display_output_addr[i] & 0xFFFFFF);
					if (ack_ptr == tmp_ptr) {
						pAviEncVidPara->display_output_addr[i] = ack_ptr;
						break;
					}
				}
			} 
			break;
#endif		
		default:
			sensor_frame = msg_id;
		#if AVI_ENCODE_DIGITAL_ZOOM_EN == 1
			scaler_frame = avi_encode_get_scaler_frame();
			scaler_zoom_once(C_SCALER_ZOOM_FIT_BUFFER,
							pAviEncVidPara->scaler_zoom_ratio,
							input_format, output_format, 
							csi_width, csi_height, 
							encode_width, encode_height,
							encode_width, encode_height, 
							sensor_frame, 0, 0, 
							scaler_frame, 0, 0);
    	#else
    		if(pAviEncVidPara->scaler_flag)
    		{   
    			scaler_frame = avi_encode_get_scaler_frame();
    			scaler_zoom_once(C_SCALER_FULL_SCREEN,
								pAviEncVidPara->scaler_zoom_ratio,
								input_format, output_format, 
								csi_width, csi_height, 
								encode_width, encode_height,
								encode_width, encode_height, 
								sensor_frame, 0, 0, 
								scaler_frame, 0, 0);
    		}
    		else
    		{
    			scaler_frame = sensor_frame;	
			}
		#endif
		 	if(AVI_ENCODE_DIGITAL_ZOOM_EN || pAviEncVidPara->scaler_flag)
		 	{
//				OSQPost(cmos_frame_q, (void *)sensor_frame);
			}
			
			
		#if AVI_ENCODE_PREVIEW_DISPLAY_EN == 1	
			if(pAviEncVidPara->dispaly_scaler_flag)
			{	
				INT32U *display_ptr;
				
				display_ptr = (INT32U *) avi_encode_get_display_frame();
				if (display_ptr && video_preview_flag) {
					if (skip_cnt == 0) {
						display_frame = *display_ptr;
						scaler_zoom_once(scaler_mode, 
										pAviEncVidPara->scaler_zoom_ratio,
										display_input_format, display_output_format, 
										encode_width, encode_height, 
										dip_width, dip_height,
										dip_buff_width, dip_buff_height, 
										scaler_frame, 0, 0, 
										display_frame, 0, 0);					
						*display_ptr |= MSG_SCALER_TASK_PREVIEW_LOCK;
						OSQPost(DisplayTaskQ, (void *) display_frame);
					}
				}
				skip_cnt++;
				if (skip_cnt > 1) {
					skip_cnt = 0;
				}
        	}
        	else
        	{
				display_frame =	scaler_frame;
			}
		#endif
#if 0 //VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE			
			if ((avi_encode_get_status() & C_AVI_VID_ENC_START) || (avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM))
			{
				if(pAviEncPara->vid_post_cnt == pAviEncPara->vid_pend_cnt)
				{
					pAviEncPara->vid_post_cnt++;
					OSQPost(vid_enc_task_q, (void *)scaler_frame);
				}
			}
	
			if((AVI_ENCODE_DIGITAL_ZOOM_EN == 0) && (pAviEncVidPara->scaler_flag ==0))
			{
				OSQPost(cmos_frame_q, (void *)sensor_frame);
			}
#endif			
		}
	}		
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//	video encode task
///////////////////////////////////////////////////////////////////////////////////////////////////////
INT32S video_encode_task_create(INT8U pori)
{
	INT8U  err;
	INT32S nRet;
	
	vid_enc_task_q = OSQCreate(video_encode_task_q_stack, C_JPEG_QUEUE_MAX);
	if(!scaler_task_q) RETURN(STATUS_FAIL);
	
	vid_enc_task_ack_m = OSMboxCreate(NULL);
	if(!scaler_task_ack_m) RETURN(STATUS_FAIL);
	
	err = OSTaskCreate(video_encode_task_entry, (void *)NULL, &JpegTaskStack[C_JPEG_STACK_SIZE-1], pori); 	
	if(err != OS_NO_ERR) RETURN(STATUS_FAIL);
		
	nRet = STATUS_OK;
Return:
	return nRet;
}

INT32S video_encode_task_del(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(vid_enc_task_q, MSG_VIDEO_ENCODE_TASK_EXIT, vid_enc_task_ack_m, 5000, msg, err);
Return:	
	OSQFlush(vid_enc_task_q);
   	OSQDel(vid_enc_task_q, OS_DEL_ALWAYS, &err);
	OSMboxDel(vid_enc_task_ack_m, OS_DEL_ALWAYS, &err);
	return nRet;
}

INT32S video_encode_task_start(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(vid_enc_task_q, MSG_VIDEO_ENCODE_TASK_MJPEG_INIT, vid_enc_task_ack_m, 5000, msg, err);
Return:
	return nRet;	
}

INT32S video_encode_task_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(vid_enc_task_q, MSG_VIDEO_ENCODE_TASK_STOP, vid_enc_task_ack_m, 5000, msg, err);
Return:
    return nRet;	
}

extern INT8U capture_ready;
void video_encode_task_entry(void *parm)
{
	INT8U   err;
	INT16U  encode_width, encode_height;
	INT16U	csi_width, csi_height;
	INT32S  header_size, encode_size;
	INT32U	output_frame, video_frame;
	INT32U  msg_id, yuv_sampling_mode;
#if MPEG4_ENCODE_ENABLE == 1
	#define MAX_P_FRAME		10
	INT8U	time_inc_bit, p_cnt;
	INT32U  temp, write_refer_addr, reference_addr; 
#endif
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	INT8U  jpeg_start_flag;
	INT16U scaler_height; 
	INT32U input_y_len, input_uv_len, blk_size;
	INT32U y_frame, u_frame, v_frame; 
	INT32S status, nRet;
	static INT8U capture_action = 0;
#else
	INT32U scaler_frame;
#endif
#if C_UVC == CUSTOM_ON
	ISOTaskMsg isosend;
#endif

	while(1)
	{
		msg_id = (INT32U) OSQPend(vid_enc_task_q, 0, &err);
		if(err != OS_NO_ERR)
		    continue;

		switch(msg_id & ~0xff)
		{
		case MSG_VIDEO_ENCODE_TASK_MJPEG_INIT:
		 	encode_width = pAviEncVidPara->encode_width;
		 	encode_height = pAviEncVidPara->encode_height;
		 	csi_width = pAviEncVidPara->sensor_capture_width;
		 	csi_height = pAviEncVidPara->sensor_capture_height;
		 	yuv_sampling_mode = C_JPEG_FORMAT_YUYV;
			output_frame = 0;
			video_frame = 0;
		#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
			pAviEncPara->vid_post_cnt = pAviEncVidPara->sensor_capture_height / SENSOR_FIFO_LINE;
			if(pAviEncVidPara->sensor_capture_height % SENSOR_FIFO_LINE)
				while(1);//this must be no remainder

			jpeg_start_flag = 0;
			scaler_height = pAviEncVidPara->sensor_capture_height / pAviEncPara->vid_post_cnt;
			input_y_len = pAviEncVidPara->sensor_capture_width * scaler_height;
			input_uv_len = input_y_len >> 1; //YUV422
			//input_uv_len = input_y_len >> 2; //YUV420
			blk_size = input_y_len << 1;
			if(pAviEncVidPara->scaler_flag)
				yuv_sampling_mode = C_JPEG_FORMAT_YUV_SEPARATE;
			else
				yuv_sampling_mode = C_JPEG_FORMAT_YUYV;
		#endif
		#if AVI_ENCODE_VIDEO_ENCODE_EN == 1
			header_size = sizeof(jpeg_avi_header);
		#endif
			OSMboxPost(vid_enc_task_ack_m, (void*)C_ACK_SUCCESS);
			break;
		case MSG_VIDEO_ENCODE_TASK_STOP:
		#if MPEG4_ENCODE_ENABLE == 1	
			if(pAviVidPara->video_format == C_XVID_FORMAT)
				avi_encode_mpeg4_free();
		#endif
			OSQFlush(vid_enc_task_q);
			OSMboxPost(vid_enc_task_ack_m, (void*)C_ACK_SUCCESS);
			break;
			
		case MSG_VIDEO_ENCODE_TASK_EXIT:
			OSMboxPost(vid_enc_task_ack_m, (void*)C_ACK_SUCCESS);	
			OSTaskDel(OS_PRIO_SELF);
			break;


		case AVIPACKER_MSG_VIDEO_WRITE_DONE:
			pAviEncVidPara->video_encode_addr[msg_id & 0xff].is_used = 0;
			JpegSendFlag = 0;

			if(time_stamp_buffer_size > 0) {
				JpegSendFlag = 1;
				first_time = pAviEncVidPara->video_encode_addr[time_stamp_buffer[0]].buffer_time;
				pfn_avi_encode_put_data(pAviEncPacker0->avi_workmem, &pAviEncVidPara->video_encode_addr[time_stamp_buffer[0]]);

				time_stamp_buffer_size--;
				if(time_stamp_buffer_size > 0) {
					INT32S i;

					for(i=0; i<time_stamp_buffer_size; i++) {
						time_stamp_buffer[i] = time_stamp_buffer[i+1];
					}
					time_stamp_buffer[i] = 0;
				} else {
					time_stamp_buffer[0] = 0;
				}
			}
			break;

		default:
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
//			pAviEncPara->vid_pend_cnt++;

			if(capture_flag) break;

			scaler_frame = msg_id;
			if ((avi_encode_get_status() & C_AVI_ENCODE_START) || (avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM)) {
				if (scaler_frame == 0x40000000) {
					DBG_PRINT("wwj: msg_id == 0x40000000\r\n");
					break;
				}
			} else {
    			goto VIDEO_ENCODE_FRAME_MODE_END;
			}

#if C_UVC == CUSTOM_ON
			if(avi_encode_get_status()&C_AVI_ENCODE_USB_WEBCAM)
			{
				video_frame = (INT32U) OSQAccept(usbwebcam_frame_q, &err);
				if(!((err == OS_NO_ERR) && video_frame))
    			{
    				goto VIDEO_ENCODE_FRAME_MODE_END;
    			}
			}
			else
#endif
			{
				video_frame = avi_encode_get_video_frame();
			}

			if(video_frame == NULL) {
				//DBG_PRINT("wwj: can't get video buffer~~\r\n");
   				goto VIDEO_ENCODE_FRAME_MODE_END;
			}

			output_frame = video_frame + header_size;
			if (s_usbd_pin == 0) {
				TIME_T	g_osd_time;
				cal_time_get(&g_osd_time);
				cpu_draw_time_osd(g_osd_time, scaler_frame, AVI_WIDTH);
			}

			if(pAviEncVidPara->video_format == C_MJPG_FORMAT)
			{
				INT8U i;

			  #if SENSOR_WIDTH==640 && AVI_WIDTH==1280
	            jpeg_enable_scale_x2_engine();
              #endif
				encode_size = jpeg_encode_once(pAviEncVidPara->quality_value, yuv_sampling_mode, 
												encode_width, encode_height, scaler_frame, 0, 0, output_frame);
#if C_UVC == CUSTOM_ON
				if(avi_encode_get_status()&C_AVI_ENCODE_USB_WEBCAM)
				{
					isosend.FrameSize = encode_size + header_size;
					isosend.AddrFrame = video_frame;
					err = OSQPost(USBCamApQ, (void*)(&isosend));//usb start send data
				} else if(avi_encode_get_status() & C_AVI_ENCODE_START)
#endif
	 			{
					/*
					OSSchedLock();
					for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++) {
						if(AviEncVidReadyFifo[i].ready_frame == 0) {
							break;
						}
					}
					if(i==AVI_ENCODE_VIDEO_BUFFER_NO) {
						DBG_PRINT("wwj: video buffer replaced!\r\n");
						i--; //replace the last one
					}
					AviEncVidReadyFifo[i].ready_frame = video_frame;
					AviEncVidReadyFifo[i].ready_size = encode_size + header_size;
					AviEncVidReadyFifo[i].key_flag = AVIIF_KEYFRAME;
					cache_invalid_range(AviEncVidReadyFifo[i].ready_frame, AviEncVidReadyFifo[i].ready_size);
					OSSchedUnlock();
					*/
				}
			}
VIDEO_ENCODE_FRAME_MODE_END:
		#if AVI_ENCODE_CSI_BUFFER_NO > 1
			if(scaler_frame != 0x40000000) {
				OSQPost(cmos_frame_q, (void *) msg_id);
			}
		#endif
			break;
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
			pAviEncPara->vid_pend_cnt++;
			if (msg_id & 0x80000000) {
				//DBG_PRINT("(%d)", pAviEncPara->vid_pend_cnt);  //test
				if (pAviEncPara->vid_pend_cnt == pAviEncPara->vid_post_cnt) {
					pAviEncPara->vid_pend_cnt = 0;
					//DBG_PRINT("e");
				} else {
					pAviEncPara->vid_pend_cnt = 0xFF;
					//DBG_PRINT("f");
				}
			}

			msg_id &= 0x7FFFFFFF;
			y_frame = msg_id;
//goto VIDEO_ENCODE_FIFO_MODE_END;

			//if (pAviEncVidPara->encode_width==1280 && pAviEncVidPara->encode_height==720 && capture_ready == 0) {
			if (pAviEncVidPara->encode_width==1280 && pAviEncVidPara->encode_height==720) {

				// start_capture_flag = 0;
				if (pAviEncPara->vid_pend_cnt == 0) {
					nRet = 3;
				}
				else if (pAviEncPara->vid_pend_cnt == 1) {
				//	jpeg_start_flag = 1;
					nRet = 1;			// First sensor image FIFO
/*
				} else if (pAviEncPara->vid_pend_cnt < 0xB) {
					nRet = 2;			// Intermediate sensor image FIFO
				} else if (pAviEncPara->vid_pend_cnt == 0xB) {
					// Draw date and time watermark in this FIFO
					if (jpeg_start_flag && !(avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM)) {
						{
							TIME_T	g_osd_time;
							cal_time_get(&g_osd_time);
							cpu_draw_time_osd(g_osd_time, y_frame, 640);
						}
					}
					nRet = 2;
				} else if (pAviEncPara->vid_pend_cnt == 0xC) {
				   
					nRet = 3;			// Last sensor image FIFO that is used
				} else if (pAviEncPara->vid_pend_cnt < pAviEncPara->vid_post_cnt) {
					nRet = 4;			// Intermediate sensor image FIFO which is not used
*/
				} else if (pAviEncPara->vid_pend_cnt < pAviEncPara->vid_post_cnt) {
					nRet = 2;			// Intermediate sensor image FIFO which is not used
				} else {
					nRet = 4;
					pAviEncPara->vid_pend_cnt = 0; 
				}
			}	
			else {	
				if (pAviEncPara->vid_pend_cnt == 0) {
					if (!(avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM)/* && jpeg_start_flag*/) {
						TIME_T	g_osd_time;

						cal_time_get(&g_osd_time);
					  #if SENSOR_WIDTH==640 && AVI_WIDTH==720
						cpu_draw_time_osd(g_osd_time, y_frame, AVI_WIDTH);
					  #else
						cpu_draw_time_osd(g_osd_time, y_frame, SENSOR_WIDTH);
					  #endif
					}
					nRet = 3;
				} else if (pAviEncPara->vid_pend_cnt == 1) {
					nRet = 1;
				} else if (pAviEncPara->vid_pend_cnt < pAviEncPara->vid_post_cnt) {
					nRet = 2;
				} else if (pAviEncPara->vid_pend_cnt == 0xFF) {
					pAviEncPara->vid_pend_cnt = 0;
					nRet = 4;
				} else {
					nRet = 4;
				}
			}	
			u_frame = v_frame = 0;

			switch(nRet)
			{
				case 1:
					if(capture_flag) {
						capture_flag = 0;
						capture_action = 1;
					}

					if (!capture_action) {
						if((avi_encode_get_status() & (C_AVI_ENCODE_USB_WEBCAM | C_AVI_ENCODE_START))) {
						  #if C_UVC == CUSTOM_ON
							if ((avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM)) {
								video_frame = (INT32U) OSQAccept(usbwebcam_frame_q, &err);
								if(err != OS_NO_ERR) {
									video_frame = 0;
				    			}
							} else 
						  #endif
							{
								video_frame = avi_encode_get_video_frame();
							}
						}

						if(!video_frame) {
							jpeg_start_flag = 0;
							//DBG_PRINT("fifo mode - wwj: can't get video buffer~~\r\n");
							//DBG_PRINT("$");
							break;
						}

						jpeg_start_flag = 1;

					  #if SENSOR_WIDTH==640 && AVI_WIDTH==1280
						jpeg_enable_scale_x2_engine();
		              #endif

						if(avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM) {
							pAviEncVidPara->video_encode_addr[g_video_index].jpeg_Q_value = AVI_Q_VALUE;
							avi_encode_set_jpeg_quality(video_frame, AVI_Q_VALUE);
							output_frame = video_frame + header_size;
						} else {
							if(curr_Q_value != pAviEncVidPara->video_encode_addr[g_video_index].jpeg_Q_value)
							{
								pAviEncVidPara->video_encode_addr[g_video_index].jpeg_Q_value = curr_Q_value;
								avi_encode_set_jpeg_quality(video_frame + 16, curr_Q_value);
							}
							output_frame = video_frame + header_size + 16;
						}

						status = jpeg_encode_fifo_start(0,
														pAviEncVidPara->quality_value,
														yuv_sampling_mode,
														encode_width, encode_height,
														y_frame, u_frame, v_frame,
														output_frame, input_y_len, input_uv_len);
						if(status < 0) {
							DEBUG_MSG(DBG_PRINT("E5"));
							goto VIDEO_ENCODE_FIFO_MODE_FAIL;
						}
					} else {
						//gp_memcpy_align((void *)write_buff_addr, (void *) y_frame, blk_size);
						#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
						INT32S i;
						#endif
						
						jpeg_header_quantization_table_calculate(0, PIC_Q_VALUE, jpeg_picture_header + 39);
						jpeg_header_quantization_table_calculate(1, PIC_Q_VALUE, jpeg_picture_header + 108);

						#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
						for(i=0; i<PHOTO_COMPRESS_CNT; i++)
						{
							capture_photo_args.jpeg_compress_buf[i].jpeg_buf_addrs = 0;
							capture_photo_args.jpeg_compress_buf[i].jpeg_vlc_size = 0;
						}

						capture_photo_args.jpeg_img_format = C_JPG_CTRL_YUV422;
						capture_photo_args.jpeg_img_width = csi_width;
						capture_photo_args.jpeg_img_height = SENSOR_FIFO_LINE;

						capture_photo_args.jpeg_compress_buf_idx = 0;
						capture_photo_args.jpeg_compress_buf_max = PHOTO_COMPRESS_CNT;
						capture_photo_args.jpeg_compress_buf[capture_photo_args.jpeg_compress_buf_idx].jpeg_buf_addrs = write_buff_addr;

						capture_photo_args.jpeg_input_addrs = y_frame;
						capture_photo_args.jpeg_input_size = blk_size;

						jpeg_encode_fifo_start_for_capture(&capture_photo_args);
						#else
						status = jpeg_encode_fifo_start(0,
														PIC_Q_VALUE,
														yuv_sampling_mode,
														encode_width, encode_height,
														y_frame, u_frame, v_frame,
														write_buff_addr + 624, input_y_len, input_uv_len);
						if(status < 0) {
							DEBUG_MSG(DBG_PRINT("E5"));
							goto VIDEO_ENCODE_FIFO_MODE_FAIL;
						}
						#endif
					}
					break;

				case 2:
					if (!capture_action) {
						if(jpeg_start_flag) {
							status = jpeg_encode_fifo_once(	0,
															status,
															y_frame, u_frame, v_frame,
															input_y_len, input_uv_len);
							if(status < 0) {
								DEBUG_MSG(DBG_PRINT("E6"));
								goto VIDEO_ENCODE_FIFO_MODE_FAIL;
							}
						}
					} else {
						//gp_memcpy_align((void *)(write_buff_addr + ((pAviEncPara->vid_pend_cnt -1)*blk_size)), (void *) y_frame, blk_size);
						#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
						capture_photo_args.jpeg_input_addrs = y_frame;
						capture_photo_args.jpeg_input_size = blk_size;
						jpeg_encode_fifo_start_for_capture(&capture_photo_args);
						#else
						status = jpeg_encode_fifo_once(	0,
														status,
														y_frame, u_frame, v_frame,
														input_y_len, input_uv_len);
						if(status < 0) {
							DEBUG_MSG(DBG_PRINT("E6"));
							goto VIDEO_ENCODE_FIFO_MODE_FAIL;
						}
						#endif

					}
					break;

				case 3:
					if (!capture_action) {
						if(jpeg_start_flag) {
							encode_size = jpeg_encode_fifo_stop(0,
																status,
																y_frame, u_frame, v_frame,
																input_y_len, input_uv_len);
							if (encode_size > 0) {

								dynamic_tune_jpeg_Q(encode_size);

								if (pAviEncVidPara->video_format == C_MJPG_FORMAT) {
								  #if C_UVC == CUSTOM_ON
									if (avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM) {
										isosend.FrameSize = encode_size + header_size;
										isosend.AddrFrame = video_frame;
										err = OSQPost(USBCamApQ, (void*)(&isosend));
									} else if(avi_encode_get_status() & C_AVI_ENCODE_START)
								  #endif
									{
										*(INT32S*)(video_frame+8) = (('0'&0xFF) | (('0'&0xFF)<<8) | (('d'&0xFF)<<16) | (('c'&0xFF)<<24));  // add "00dc"
										*(INT32S*)(video_frame+12) = (((encode_size + header_size + 8 + 511)>>9)<<9) - 8; // add length

										pAviEncVidPara->video_encode_addr[g_video_index].msg_id = AVIPACKER_MSG_VIDEO_WRITE;
										pAviEncVidPara->video_encode_addr[g_video_index].buffer_len = ((encode_size + header_size + 8 + 511)>>9)<<9;//AVI_ENCODE_VIDEO_BUFFER_SIZE;
										pAviEncVidPara->video_encode_addr[g_video_index].buffer_time = OSTimeGet();

										time_stamp_buffer[time_stamp_buffer_size] = g_video_index;
										time_stamp_buffer_size++;

										if(JpegSendFlag == 0)
										{
											JpegSendFlag = 1;
											first_time = pAviEncVidPara->video_encode_addr[time_stamp_buffer[0]].buffer_time;
											pfn_avi_encode_put_data(pAviEncPacker0->avi_workmem, &pAviEncVidPara->video_encode_addr[time_stamp_buffer[0]]);

											time_stamp_buffer_size--;
											if(time_stamp_buffer_size > 0) {
												INT32S i;

												for(i=0; i<time_stamp_buffer_size; i++) {
													time_stamp_buffer[i] = time_stamp_buffer[i+1];
												}
												time_stamp_buffer[i] = 0;
											} else {
												time_stamp_buffer[0] = 0;
											}
										}
									}
									jpeg_start_flag = video_frame = 0;
								}
							} else {
								DEBUG_MSG(DBG_PRINT("E7"));
								goto VIDEO_ENCODE_FIFO_MODE_FAIL;
							}
						}
					} else {
					   #if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
						capture_action = 0;
						capture_photo_args.jpeg_input_addrs = y_frame;
						capture_photo_args.jpeg_input_size = blk_size;

						jpeg_encode_fifo_start_for_capture(&capture_photo_args);

						OSQPost(AVIEncodeApQ, (void *) MSG_AVI_CAPTURE_PICTURE);
						//POST_MESSAGE(AVIEncodeApQ, MSG_AVI_CAPTURE_PICTURE, avi_encode_ack_m, 5000, msg, err);
						break;
						#else
						encode_size = jpeg_encode_fifo_stop(0,
															status,
															y_frame, u_frame, v_frame,
															input_y_len, input_uv_len);
						if (encode_size > 0) {
							capture_action = 0;
							jpeg_encode_stop(); 
							*(INT32S*)write_buff_addr = encode_size;
							OSQPost(AVIEncodeApQ, (void *) MSG_AVI_CAPTURE_PICTURE);
							//POST_MESSAGE(AVIEncodeApQ, MSG_AVI_CAPTURE_PICTURE, avi_encode_ack_m, 5000, msg, err);
							break;
						} else {
							DEBUG_MSG(DBG_PRINT("E7"));
							goto VIDEO_ENCODE_FIFO_MODE_FAIL;
						}
						#endif
					}
					break;
				case 4:
					if(capture_action) {
						capture_action = 0;
						capture_flag = 1;
					}
					goto	VIDEO_ENCODE_FIFO_MODE_FAIL;
					break;
			}
VIDEO_ENCODE_FIFO_MODE_END:
			OSQPost(cmos_frame_q, (void *)y_frame);
			break;

VIDEO_ENCODE_FIFO_MODE_FAIL:
			//DEBUG_MSG(DBG_PRINT("E"));
			if(jpeg_start_flag == 1)
			{
				jpeg_encode_stop();
				if ((avi_encode_get_status() & C_AVI_ENCODE_USB_WEBCAM) && video_frame) {
				    OSQPost(usbwebcam_frame_q, (void *)video_frame);
	    		}
				pAviEncVidPara->video_encode_addr[g_video_index].is_used = 0;
				jpeg_start_flag = video_frame = 0;
			}
			goto	VIDEO_ENCODE_FIFO_MODE_END;
#endif
			break;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
