#include "avi_encoder_state.h"
 
#define C_AVI_ENCODE_STATE_STACK_SIZE   512
#define AVI_ENCODE_QUEUE_MAX_LEN    	60

#define C_AVI_DELETE_STATE_STACK_SIZE   1024
#define C_DELETE_QUEUE_MAX    	5

/* OS stack */
INT32U      AVIEncodeStateStack[C_AVI_ENCODE_STATE_STACK_SIZE];

/* global varaible */
OS_EVENT *AVIEncodeApQ;
OS_EVENT *avi_encode_ack_m;
void *AVIEncodeApQ_Stack[AVI_ENCODE_QUEUE_MAX_LEN];
#if C_UVC == CUSTOM_ON
OS_EVENT *usbwebcam_frame_q;
void *usbwebcam_frame_q_stack[AVI_ENCODE_VIDEO_BUFFER_NO];
#endif

INT32S vid_enc_preview_start(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	// start scaler
	if((avi_encode_get_status()&C_AVI_ENCODE_SCALER) == 0)
	{
		if(scaler_task_start() < 0) RETURN(STATUS_FAIL);
		avi_encode_set_status(C_AVI_ENCODE_SCALER);
		DEBUG_MSG(DBG_PRINT("a.scaler start\r\n"));
	}
	// start video
#if AVI_ENCODE_VIDEO_ENCODE_EN == 1 
	if((avi_encode_get_status()&C_AVI_ENCODE_VIDEO) == 0)
	{
		if(video_encode_task_start() < 0) RETURN(STATUS_FAIL);
		avi_encode_set_status(C_AVI_ENCODE_VIDEO);
		DEBUG_MSG(DBG_PRINT("b.video start\r\n"));
	}
#endif
	// start sensor
	if((avi_encode_get_status()&C_AVI_ENCODE_SENSOR) == 0)
	{
		POST_MESSAGE(AVIEncodeApQ, MSG_AVI_START_SENSOR, avi_encode_ack_m, 5000, msg, err);
		avi_encode_set_status(C_AVI_ENCODE_SENSOR);
		DEBUG_MSG(DBG_PRINT("c.sensor start\r\n"));
	}
	// start audio 
#if AVI_ENCODE_PRE_ENCODE_EN == 1
	if(pAviEncAudPara->audio_format && ((avi_encode_get_status()&C_AVI_ENCODE_AUDIO) == 0))
	{
		if(avi_audio_record_start() < 0) RETURN(STATUS_FAIL);
		avi_encode_set_status(C_AVI_VID_ENC_START);
		avi_encode_set_status(C_AVI_ENCODE_AUDIO);
		avi_encode_audio_timer_start();
		DEBUG_MSG(DBG_PRINT("d.audio start\r\n"));
	}
#endif	
Return:	
	return nRet;
}

INT32S vid_enc_preview_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	// stop audio	
	if(avi_encode_get_status()&C_AVI_ENCODE_AUDIO)
	{
		if(avi_audio_record_stop() < 0) RETURN(STATUS_FAIL);
		avi_encode_clear_status(C_AVI_ENCODE_AUDIO);
		avi_encode_audio_timer_stop();
		DEBUG_MSG(DBG_PRINT("a.audio stop\r\n"));
	}
	// stop sensor
	if(avi_encode_get_status()&C_AVI_ENCODE_SENSOR)
	{
		POST_MESSAGE(AVIEncodeApQ, MSG_AVI_STOP_SENSOR, avi_encode_ack_m, 5000, msg, err);	
		avi_encode_clear_status(C_AVI_ENCODE_SENSOR);
		DEBUG_MSG(DBG_PRINT("b.sensor stop\r\n"));
	}	
	// stop video
	if(avi_encode_get_status()&C_AVI_ENCODE_VIDEO)
	{	
		if(video_encode_task_stop() < 0) RETURN(STATUS_FAIL);
		avi_encode_clear_status(C_AVI_ENCODE_VIDEO);
		DEBUG_MSG(DBG_PRINT("c.video stop\r\n"));
	}
	// stop scaler
	if(avi_encode_get_status()&C_AVI_ENCODE_SCALER)
	{
		if(scaler_task_stop() < 0) RETURN(STATUS_FAIL); 
		avi_encode_clear_status(C_AVI_ENCODE_SCALER);
		DEBUG_MSG(DBG_PRINT("d.scaler stop\r\n"));  
	}
Return:	
	return nRet;
}

INT32S avi_enc_start(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	// start audio 
	if(pAviEncAudPara->audio_format && ((avi_encode_get_status()&C_AVI_ENCODE_AUDIO) == 0))
	{
		if(avi_audio_record_start() < 0) RETURN(STATUS_FAIL);
		avi_encode_set_status(C_AVI_ENCODE_AUDIO);
		DEBUG_MSG(DBG_PRINT("b.audio start\r\n"));
	}
	// restart audio 
#if AVI_ENCODE_PRE_ENCODE_EN == 1
	if(pAviEncAudPara->audio_format && (avi_encode_get_status()&C_AVI_ENCODE_AUDIO))
	{
		if(avi_audio_record_restart() < 0) RETURN(STATUS_FAIL);
		DEBUG_MSG(DBG_PRINT("b.audio restart\r\n"));
	}
#endif
	// start avi encode
	if((avi_encode_get_status()&C_AVI_ENCODE_START) == 0)
	{
		POST_MESSAGE(AVIEncodeApQ, MSG_AVI_START_ENCODE, avi_encode_ack_m, 5000, msg, err);	
		avi_encode_set_status(C_AVI_ENCODE_START);
		avi_encode_set_status(C_AVI_VID_ENC_START);
		DEBUG_MSG(DBG_PRINT("c.encode start\r\n")); 
	}
Return:	
	return nRet;
}

INT32S avi_enc_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;

	nRet = STATUS_OK;
#if AVI_ENCODE_PRE_ENCODE_EN == 0
	// stop audio
	if(avi_encode_get_status()&C_AVI_ENCODE_AUDIO)
	{
		if(avi_audio_record_stop() < 0) RETURN(STATUS_FAIL);
		avi_encode_clear_status(C_AVI_ENCODE_AUDIO);
		DEBUG_MSG(DBG_PRINT("a.audio stop\r\n"));
	}
#endif
	// stop avi encode
	if(avi_encode_get_status()&C_AVI_ENCODE_START)
	{
		POST_MESSAGE(AVIEncodeApQ, MSG_AVI_STOP_ENCODE, avi_encode_ack_m, 5000, msg, err);
		avi_encode_clear_status(C_AVI_ENCODE_START);
#if AVI_ENCODE_PRE_ENCODE_EN == 0
		avi_encode_clear_status(C_AVI_VID_ENC_START);
#endif
		DEBUG_MSG(DBG_PRINT("b.encode stop\r\n")); 
	}
Return:	
	return nRet;
}

INT32S avi_enc_pause(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	if(avi_encode_get_status()&C_AVI_ENCODE_START)
    {
    	POST_MESSAGE(AVIEncodeApQ, MSG_AVI_PAUSE_ENCODE, avi_encode_ack_m, 5000, msg, err);	
    	avi_encode_set_status(C_AVI_ENCODE_PAUSE);
		DEBUG_MSG(DBG_PRINT("encode pause\r\n")); 
    }
Return:    
	return nRet;
}

INT32S avi_enc_resume(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	if(avi_encode_get_status()&(C_AVI_ENCODE_START|C_AVI_ENCODE_PAUSE))
    {
    	POST_MESSAGE(AVIEncodeApQ, MSG_AVI_RESUME_ENCODE, avi_encode_ack_m, 5000, msg, err);	
    	avi_encode_clear_status(C_AVI_ENCODE_PAUSE);
		DEBUG_MSG(DBG_PRINT("encode resume\r\n"));  
    }
Return:    
	return nRet;
}

INT32S avi_enc_save_jpeg(void)
{
	//INT8U  err;
	INT32S nRet;//, msg;
	capture_flag = 1;
	nRet = STATUS_OK;

#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	OSQPost(AVIEncodeApQ, (void *) MSG_AVI_CAPTURE_PICTURE);
#endif
	return nRet;
}

#if C_UVC == CUSTOM_ON
INT32S usb_webcam_start(void)
{

	INT8U  err;
	INT32S nRet, msg;
	INT32S nTemp;
	
	if(avi_encode_get_status()&C_AVI_ENCODE_START)
	{
		if(avi_encode_get_status() & C_AVI_ENCODE_PAUSE)
		 	nRet = avi_enc_resume();
		 
    	//stop avi encode	
    	nRet = avi_enc_stop();
   		 //stop avi packer
    	nTemp = avi_enc_packer_stop(pAviEncPara->AviPackerCur);
    
    	if(nRet < 0 || nTemp < 0)
    		return CODEC_START_STATUS_ERROR_MAX;
	}
	nRet = STATUS_OK;
	POST_MESSAGE(AVIEncodeApQ, MSG_AVI_START_USB_WEBCAM, avi_encode_ack_m, 5000, msg, err);
	avi_encode_set_status(C_AVI_ENCODE_USB_WEBCAM);
#if C_USB_AUDIO == CUSTOM_ON
	avi_audio_record_start();
	avi_encode_audio_timer_start();
#endif
Return:    
	return nRet;
}
INT32S usb_webcam_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	avi_encode_clear_status(C_AVI_ENCODE_USB_WEBCAM);
	nRet = STATUS_OK;
	POST_MESSAGE(AVIEncodeApQ, MSG_AVI_STOP_USB_WEBCAM, avi_encode_ack_m, 5000, msg, err);
#if C_USB_AUDIO == CUSTOM_ON	
	avi_audio_record_stop();
	avi_encode_audio_timer_stop();
#endif
Return:    
	return nRet;
}
#endif
//======================================================================================================
INT32S avi_encode_state_task_create(INT8U pori)
{
	INT8U err;
	INT32S nRet;
	
	AVIEncodeApQ = OSQCreate(AVIEncodeApQ_Stack, AVI_ENCODE_QUEUE_MAX_LEN);
    if(!AVIEncodeApQ)  RETURN(STATUS_FAIL);
#if C_UVC == CUSTOM_ON
    usbwebcam_frame_q = OSQCreate(usbwebcam_frame_q_stack, AVI_ENCODE_VIDEO_BUFFER_NO);
    if(!usbwebcam_frame_q)  RETURN(STATUS_FAIL);
#endif
    avi_encode_ack_m = OSMboxCreate(NULL);
	if(!avi_encode_ack_m) RETURN(STATUS_FAIL);
	
	err = OSTaskCreate(avi_encode_state_task_entry, (void*) NULL, &AVIEncodeStateStack[C_AVI_ENCODE_STATE_STACK_SIZE - 1], pori);
	if(err != OS_NO_ERR) RETURN(STATUS_FAIL);
	
    nRet = STATUS_OK;
Return:
    return nRet;
}

INT32S avi_encode_state_task_del(void)
{
    INT8U   err;
    INT32S  nRet, msg;
   
    nRet = STATUS_OK; 
 	POST_MESSAGE(AVIEncodeApQ, MSG_AVI_ENCODE_STATE_EXIT, avi_encode_ack_m, 5000, msg, err);	
 Return:	
   	OSQFlush(AVIEncodeApQ);
   	OSQDel(AVIEncodeApQ, OS_DEL_ALWAYS, &err);
	OSMboxDel(avi_encode_ack_m, OS_DEL_ALWAYS, &err);
    return nRet;
}

INT32S avi_enc_storage_full(void)
{
	DEBUG_MSG(DBG_PRINT("avi encode storage full!!!\r\n"));
	avi_encode_video_timer_stop();
	if(OSQPost(AVIEncodeApQ, (void*)MSG_AVI_STORAGE_FULL) != OS_NO_ERR)
		return STATUS_FAIL;

	return STATUS_OK;
} 

INT32S avi_packer_err_handle(INT32S ErrCode)
{
	INT8U flag;

	avi_encode_video_timer_stop();
	if(ErrCode == AVIPACKER_RESULT_FILE_WRITE_ERR || ErrCode == AVIPACKER_RESULT_IDX_FILE_WRITE_ERR) {
		DEBUG_MSG(DBG_PRINT("AviPacker-ErrID = 0x%x!!!\r\n", ErrCode));
		flag = 0;
		msgQSend(ApQ, MSG_APQ_AVI_PACKER_ERROR, &flag, sizeof(INT8U), MSG_PRI_NORMAL);
	}
	else if(ErrCode == AVIPACKER_RESULT_CLUSTER_GRP_END)
	{
		flag = 1;
		msgQSend(ApQ, MSG_APQ_AVI_PACKER_ERROR, &flag, sizeof(INT8U), MSG_PRI_NORMAL);
	}
	return 1; 	//return 1 to close task
}

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
extern capture_photo_args_t capture_photo_args;
#endif
void avi_encode_state_task_entry(void *para)
{
	INT8U   err, i/*, success_flag, key_flag*/;
    INT32S  nRet, N;
    INT32U  msg_id, video_frame, video_stream;
	#if !((PIC_WIDTH == 2560) && (PIC_HEIGH == 1440))
	INT32U encode_size;
  	#endif
  #if VIDEO_ENCODE_USE_MODE	== SENSOR_BUF_FRAME_MODE && AVI_ENCODE_CSI_BUFFER_NO > 1
	INT32U	addr;
  #endif
    //INT64S  dwtemp;	
	//OS_CPU_SR cpu_sr;

    while(1)
    {
        msg_id = (INT32U) OSQPend(AVIEncodeApQ, 0, &err);
        if((!msg_id) || (err != OS_NO_ERR))
        	continue;
        	
        switch(msg_id)
        {
        case MSG_AVI_START_SENSOR:	//sensor
        DBG_PRINT("sen_start\r\n");
          	jpeg_picture_header[7] = (PIC_HEIGH >> 8) & 0xFF;
			jpeg_picture_header[8] = PIC_HEIGH & 0xFF;
			jpeg_picture_header[9] = (PIC_WIDTH >> 8) & 0xFF;
			jpeg_picture_header[10] = PIC_WIDTH & 0xFF;
			jpeg_picture_header[0xB0] = (INT8U)((((PIC_WIDTH + 15) & 0xFFF0)>>2)>>8);
			jpeg_picture_header[0xB1] = (INT8U)((((PIC_WIDTH + 15) & 0xFFF0)>>2) & 0xFF);
          	OSQFlush(cmos_frame_q);
        #if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
        	video_frame = avi_encode_get_csi_frame();
			video_stream = 0;
		  #if AVI_ENCODE_CSI_BUFFER_NO > 1
			for(nRet = 1; nRet<AVI_ENCODE_CSI_BUFFER_NO; nRet++)
				OSQPost(cmos_frame_q, (void *) avi_encode_get_csi_frame());
		  #endif
		#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
			for(nRet = 0; nRet<AVI_ENCODE_CSI_BUFFER_NO; nRet++) {
				OSQPost(cmos_frame_q, (void *) avi_encode_get_csi_frame());
			}
			video_frame = (INT32U) OSQAccept(cmos_frame_q, &err);
			video_stream = (INT32U) OSQAccept(cmos_frame_q, &err);
		#endif
	        nRet = video_encode_sensor_start(video_frame, video_stream);
	        if(nRet >= 0)
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
        	else
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_FAIL);
            break;
        case MSG_AVI_STOP_SENSOR: 
            nRet = video_encode_sensor_stop();
            OSQFlush(cmos_frame_q);
            if(nRet >= 0)
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
        	else
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_FAIL);
            break;   
#if C_UVC == CUSTOM_ON            
        case MSG_AVI_START_USB_WEBCAM:	//sensor
          	OSQFlush(usbwebcam_frame_q);
			for(nRet = 0; nRet<AVI_ENCODE_VIDEO_BUFFER_NO; nRet++)
				OSQPost(usbwebcam_frame_q, (void *) avi_encode_get_video_frame());	

			for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++) {
				time_stamp_buffer[i] = 0;
				pAviEncVidPara->video_encode_addr[i].is_used = 0;
				pAviEncVidPara->video_encode_addr[i].jpeg_Q_value = 0;
			}
	        if(nRet >= 0)
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
        	else
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_FAIL);
            break;
        
        case MSG_AVI_STOP_USB_WEBCAM: 
        
            OSQFlush(usbwebcam_frame_q);
            if(nRet >= 0)
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
        	else
            	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_FAIL);
            break;
#endif             
        case MSG_AVI_START_ENCODE: //avi encode start
			nRet = pAviEncVidPara->dwRate/pAviEncVidPara->dwScale;
			nRet /= 4;	//wwj add

			N = 0;
			while(nRet >>= 1) N++; 

			pAviEncPara->ta = 0;
			pAviEncPara->tv = 0;
			pAviEncPara->Tv = 0;
			pAviEncPara->pend_cnt = 0;
			pAviEncPara->post_cnt = 0;
			pAviEncPara->ready_frame = 0;
			pAviEncPara->ready_size = 0;
			if(pAviEncPara->AviPackerCur->p_avi_wave_info)
			{
				pAviEncPara->freq_div = pAviEncAudPara->audio_sample_rate/AVI_ENCODE_TIME_BASE;
				pAviEncPara->tick = (INT64S)pAviEncVidPara->dwRate * pAviEncPara->freq_div;
				pAviEncPara->delta_Tv = pAviEncVidPara->dwScale * pAviEncAudPara->audio_sample_rate;
			}
			else
			{
				pAviEncPara->freq_div = 1;
				pAviEncPara->tick = (INT64S)pAviEncVidPara->dwRate * pAviEncPara->freq_div;
				pAviEncPara->delta_Tv = pAviEncVidPara->dwScale * AVI_ENCODE_TIME_BASE;
			}

			JpegSendFlag = 0;
			time_stamp_buffer_size = 0;
			for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++) {
				time_stamp_buffer[i] = 0;
				pAviEncVidPara->video_encode_addr[i].is_used = 0;
				pAviEncVidPara->video_encode_addr[i].jpeg_Q_value = 0;
			}

		case MSG_AVI_RESUME_ENCODE:
		#if AVI_ENCODE_PRE_ENCODE_EN == 0
			if(pAviEncPara->AviPackerCur->p_avi_wave_info)
				avi_encode_audio_timer_start();
        #endif	
        	//avi_encode_video_timer_start();
            OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
            break;

        case MSG_AVI_STOP_ENCODE:
        case MSG_AVI_PAUSE_ENCODE:
        #if AVI_ENCODE_PRE_ENCODE_EN == 0
        	if(pAviEncPara->AviPackerCur->p_avi_wave_info) 
        		avi_encode_audio_timer_stop();
        #endif	
        	avi_encode_video_timer_stop();
            OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
            break;

        case MSG_AVI_CAPTURE_PICTURE:
        	{
	        	INT8S flag = 0;
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
				TIME_T	g_osd_time;
				INT32U frame;
#endif
				#if !((PIC_WIDTH == 2560) && (PIC_HEIGH == 1440))
				encode_size = *(INT32S *)write_buff_addr;
				#endif
				jpeg_header_quantization_table_calculate(0, PIC_Q_VALUE, jpeg_picture_header + 39);
				jpeg_header_quantization_table_calculate(1, PIC_Q_VALUE, jpeg_picture_header + 108);
				
				#if SENSOR_WIDTH==PIC_WIDTH && SENSOR_HEIGHT==PIC_HEIGH
    				jpeg_picture_header[0xB0]=0;
    				jpeg_picture_header[0xB1]=0;
    			#else
					#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
    				jpeg_picture_header[0xB0] = (INT8U)(640 >> 8);
					jpeg_picture_header[0xB1] = (INT8U)(640 & 0xFF);
					#else
    				jpeg_picture_header[0xB0] = (INT8U)((((PIC_WIDTH + 15) & 0xFFF0)>>2)>>8);
					jpeg_picture_header[0xB1] = (INT8U)((((PIC_WIDTH + 15) & 0xFFF0)>>2) & 0xFF);
					#endif
    			#endif

video_encode_sensor_stop();
OSTimeDly(10);
				
				write(pAviEncPara->AviPackerCur->file_handle, (INT32U) jpeg_picture_header, 624);
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
				cal_time_get(&g_osd_time);
				do {
					frame = *P_CSI_TG_FBSADDR;
				} while (frame != 0x40000000);

				cpu_draw_time_osd(g_osd_time, pAviEncVidPara->csi_frame_addr[0], pAviEncVidPara->sensor_capture_width);
			  #if SENSOR_WIDTH==PIC_WIDTH && SENSOR_HEIGHT==PIC_HEIGH
                encode_size=jpeg_encode_once(PIC_Q_VALUE, C_JPEG_FORMAT_YUYV, PIC_WIDTH, PIC_HEIGH, pAviEncVidPara->csi_frame_addr[0], 0, 0, write_buff_addr);
				write(pAviEncPara->AviPackerCur->file_handle, write_buff_addr, encode_size);
			  #else
				scale_up_and_encode(pAviEncVidPara->csi_frame_addr[0], write_buff_addr, PIC_WIDTH, PIC_HEIGH, pAviEncVidPara->video_encode_addr[0] + 624);
			  #endif

			  #if AVI_ENCODE_CSI_BUFFER_NO > 1
	        	OS_ENTER_CRITICAL();
				addr = (INT32U) OSQAccept(cmos_frame_q, &err);
				while(!((addr == NULL) || (err != OS_NO_ERR))) {
					addr = (INT32U) OSQAccept(cmos_frame_q, &err);
				}

				for(nRet = 0; nRet<AVI_ENCODE_CSI_BUFFER_NO; nRet++) {
					OSQPost(cmos_frame_q, (void *) avi_encode_get_csi_frame());
				}
	        	OS_EXIT_CRITICAL();
	          #endif
#else
              #if SENSOR_WIDTH==PIC_WIDTH && SENSOR_HEIGHT==PIC_HEIGH
				//encode_size=jpeg_encode_once(PIC_Q_VALUE, C_JPEG_FORMAT_YUYV, PIC_WIDTH, PIC_HEIGH,write_buff_addr, 0, 0, pAviEncVidPara->video_encode_addr[0] + 624);
			    //memcpy(pAviEncVidPara->video_encode_addr[0],jpeg_picture_header,624);
				//write(pAviEncPara->AviPackerCur->file_handle, (INT32U)(pAviEncVidPara->video_encode_addr[0] + 624), encode_size);
				write(pAviEncPara->AviPackerCur->file_handle, (INT32U)(write_buff_addr + 624), encode_size);
			  #elif SENSOR_WIDTH==640 && AVI_WIDTH==720		// Reserve 720x480x2 for sensor buffer
				scale_up_and_encode(write_buff_addr, write_buff_addr + 720*SENSOR_HEIGHT*2, PIC_WIDTH, PIC_HEIGH, pAviEncVidPara->video_encode_addr[0] + 624);
			  #else
				//scale_up_and_encode(write_buff_addr, write_buff_addr + SENSOR_WIDTH*SENSOR_HEIGHT*2, PIC_WIDTH, PIC_HEIGH, pAviEncVidPara->video_encode_addr[0].buffer_addrs + 624);
				//scale_up_and_encode(write_buff_addr, write_buff_addr + SENSOR_WIDTH*SENSOR_HEIGHT*2, PIC_WIDTH, PIC_HEIGH, write_buff_addr + SENSOR_WIDTH*SENSOR_HEIGHT*2 + PIC_WIDTH*16*2*2 + 624); //A/B buffer
				#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
				capture_photo_args.jpeg_file_handle = pAviEncPara->AviPackerCur->file_handle;
				scale_up_and_encode_for_capture(&capture_photo_args);
				#else
				scale_up_and_encode(write_buff_addr, write_buff_addr + SENSOR_WIDTH*SENSOR_HEIGHT*2, PIC_WIDTH, PIC_HEIGH, write_buff_addr + SENSOR_WIDTH*SENSOR_HEIGHT*2 + PIC_WIDTH*16*2*2 + 624); //A/B buffer
				#endif
			  #endif
#endif
				capture_flag = 0;
				if (close(pAviEncPara->AviPackerCur->file_handle)) {
					nRet = STATUS_FAIL;
				} else {
					nRet = STATUS_OK;
				}
				if(nRet < 0) goto AVI_CAPTURE_PICTURE_FAIL;
				msgQSend(ApQ, MSG_STORAGE_SERVICE_PIC_DONE, &flag, sizeof(INT8S), MSG_PRI_NORMAL);
	            OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS);
	       		break;
AVI_CAPTURE_PICTURE_FAIL: 
	        	flag = -1;
	        	msgQSend(ApQ, MSG_STORAGE_SERVICE_PIC_DONE, &flag, sizeof(INT8S), MSG_PRI_NORMAL);
	        	OSMboxPost(avi_encode_ack_m, (void*)C_ACK_FAIL);
	        	break;
			}

		/*
		case MSG_AVI_STORAGE_FULL:
       		{
       			INT32U led_type;
				
				led_type = LED_WAITING_CAPTURE;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_FLASH_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
       		}

       	case MSG_AVI_STORAGE_ERR:
       	#if AVI_ENCODE_PRE_ENCODE_EN == 0
       		if(pAviEncPara->AviPackerCur->p_avi_wave_info) 
       			avi_audio_record_stop();
	       	video_encode_task_stop();
	       	
	    	if(pAviEncPara->source_type == SOURCE_TYPE_FS)
        	{
        		AviPackerV3_Close(pAviEncPara->AviPackerCur->avi_workmem);     
        		avi_encode_close_file(pAviEncPara->AviPackerCur);
        	}
        	
        	msg_id = C_AVI_VID_ENC_START|C_AVI_ENCODE_AUDIO|C_AVI_ENCODE_VIDEO|C_AVI_ENCODE_START|C_AVI_ENCODE_PAUSE;
       		if(pAviEncPara->AviPackerCur == pAviEncPacker0)
       			msg_id |= C_AVI_ENCODE_PACKER0; 
       		else
       			msg_id |= C_AVI_ENCODE_PACKER1; 
       		
        	avi_encode_clear_status(msg_id);
	    #else
	    	msg_id = C_AVI_ENCODE_START|C_AVI_ENCODE_PAUSE;
       		if(pAviEncPara->AviPackerCur == pAviEncPacker0)
       			msg_id |= C_AVI_ENCODE_PACKER0;
       		else
       			msg_id |= C_AVI_ENCODE_PACKER1;
       		
       		if(pAviEncPara->source_type == SOURCE_TYPE_FS)
        	{
        		AviPackerV3_Close(pAviEncPara->AviPackerCur->avi_workmem);  
        		avi_encode_close_file(pAviEncPara->AviPackerCur);
        	}

	    	avi_encode_clear_status(msg_id);
		#endif
            OSQFlush(AVIEncodeApQ);
            OSQFlush(avi_encode_ack_m);
            break;
            
       	case MSG_AVI_ENCODE_STATE_EXIT:
       		OSMboxPost(avi_encode_ack_m, (void*)C_ACK_SUCCESS); 
   			OSTaskDel(OS_PRIO_SELF);
       		break;

       	case MSG_AVI_ONE_FRAME_ENCODE:
        	success_flag = 0;
        	OS_ENTER_CRITICAL();
			dwtemp = (INT64S)(pAviEncPara->tv - pAviEncPara->Tv);
			OS_EXIT_CRITICAL();
			if(dwtemp > (pAviEncPara->delta_Tv << N))
				goto EncodeNullFrame;

			if(AviEncVidReadyFifo[0].ready_frame == NULL) {
				goto EndofEncodeOneFrame;
			}

			video_stream = AviEncVidReadyFifo[0].ready_size + 8 + 2*16;
		#if C_AUTO_DEL_FILE	== CUSTOM_OFF
			nRet = avi_encode_disk_size_is_enough(video_stream);
			if (nRet == 0) {
				avi_enc_storage_full();
				goto EndofEncodeOneFrame;
			} else if (nRet == 2) {
				msgQSend(ApQ, MSG_APQ_RECORD_SWITCH_FILE, NULL, NULL, MSG_PRI_NORMAL);
				goto EndofEncodeOneFrame;
			}
		#endif
			video_frame = AviEncVidReadyFifo[0].ready_frame;
			encode_size = AviEncVidReadyFifo[0].ready_size;
			key_flag = AviEncVidReadyFifo[0].key_flag;
			nRet = pfn_avi_encode_put_data(	pAviEncPara->AviPackerCur->avi_workmem,
											*(long*)"00dc", 
											encode_size, 
											(void *)video_frame, 
											1, 
											key_flag);
			OSSchedLock();
			for(i=0; i<(AVI_ENCODE_VIDEO_BUFFER_NO-1); i++) {
				AviEncVidReadyFifo[i] = AviEncVidReadyFifo[i+1];
			}
			AviEncVidReadyFifo[i].ready_frame = NULL;
			OSSchedUnlock();

			if(nRet >= 0)
			{
				DEBUG_MSG(DBG_PRINT("."));
				success_flag = 1;
				goto EndofEncodeOneFrame;
			}
			else
			{
				avi_encode_disk_size_is_enough(-video_stream);
				DEBUG_MSG(DBG_PRINT("VidPutData = %x, size = %d !!!\r\n", nRet-0x80000000, encode_size));
			}
EncodeNullFrame:
			avi_encode_disk_size_is_enough(8+2*16);
			nRet = pfn_avi_encode_put_data(	pAviEncPara->AviPackerCur->avi_workmem,
											*(long*)"00dc", 
											0, 
											(void *)NULL, 
											1, 
											0x00);
			if(nRet >= 0)
			{
				DEBUG_MSG(DBG_PRINT("N"));
				success_flag = 1;
			}
			else
			{
				avi_encode_disk_size_is_enough(-(8+2*16));
				DEBUG_MSG(DBG_PRINT("VidPutDataNULL = %x!!!\r\n", nRet-0x80000000));
			}
EndofEncodeOneFrame:
			if(success_flag)
			{
				OS_ENTER_CRITICAL();
				pAviEncPara->Tv += pAviEncPara->delta_Tv;
				OS_EXIT_CRITICAL();
			}
			pAviEncPara->pend_cnt++;
        	break;
        	*/
        } 
    }
}