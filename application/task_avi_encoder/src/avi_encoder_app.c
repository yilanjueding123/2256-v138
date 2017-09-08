#include "avi_encoder_app.h"
#include "jpeg_header.h"
#include "video_codec_callback.h"
#include "avi_encoder_scaler_jpeg.h"

/* global varaible */
AviEncPara_t AviEncPara, *pAviEncPara;
AviEncAudPara_t AviEncAudPara, *pAviEncAudPara;
AviEncVidPara_t AviEncVidPara, *pAviEncVidPara;
AviEncPacker_t AviEncPacker0, *pAviEncPacker0;
AviEncPacker_t AviEncPacker1, *pAviEncPacker1;
INT8U JpegSendFlag = 0;
INT32U first_time = 0;
INT32S time_stamp_buffer_size = 0;
INT32U time_stamp_buffer[AVI_ENCODE_VIDEO_BUFFER_NO] = {0};

GP_AVI_AVISTREAMHEADER	avi_aud_stream_header;
GP_AVI_AVISTREAMHEADER	avi_vid_stream_header;
GP_AVI_BITMAPINFO		avi_bitmap_info;
GP_AVI_PCMWAVEFORMAT	avi_wave_info;

static DMA_STRUCT g_avi_adc_dma_dbf;

static INT8U g_csi_index;
static INT8U g_scaler_index;
static INT8U g_display_index;
/*static*/ INT8U g_video_index;
static INT8U g_pcm_index;

void  (*pfn_avi_encode_put_data)(void *WorkMem, AVIPACKER_FRAME_INFO* pFrameInfo);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// avi encode api	
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static void scaler_lock(void)
{
	INT8U err;
	OSSemPend(scaler_hw_sem, 0, &err);
}

static void scaler_unlock(void)
{
	OSSemPost(scaler_hw_sem);
}

void avi_encode_init(void)
{
    pAviEncPara = &AviEncPara;
    gp_memset((INT8S *)pAviEncPara, 0, sizeof(AviEncPara_t));
    
    pAviEncAudPara = &AviEncAudPara;
    gp_memset((INT8S *)pAviEncAudPara, 0, sizeof(AviEncAudPara_t));
	pAviEncVidPara = &AviEncVidPara;
    gp_memset((INT8S *)pAviEncVidPara, 0, sizeof(AviEncVidPara_t));

    pAviEncPacker0 = &AviEncPacker0;
    gp_memset((INT8S *)pAviEncPacker0, 0, sizeof(AviEncPacker_t));   
    pAviEncPacker1 = &AviEncPacker1;
    gp_memset((INT8S *)pAviEncPacker1, 0, sizeof(AviEncPacker_t));
    
	pAviEncPacker0->file_handle = -1;
	pAviEncPacker1->file_handle = -1;
	pAviEncPacker0->print_flag = 1;
	pAviEncPacker1->print_flag = 0;
	pAviEncVidPara->scaler_zoom_ratio = 1;
}


/****************************************************************************/
/*
 *	avi_enc_packer_init
 */
INT32U avi_enc_packer_init(AviEncPacker_t *pAviEncPacker)
{
	INT32S nRet;

	if(pAviEncPacker == pAviEncPacker0) {
		pAviEncPacker->task_prio = AVI_PACKER0_PRIORITY;
	} else {
		RETURN(STATUS_FAIL);
	}
	
	nRet = AviPackerV3_TaskCreate(pAviEncPacker->task_prio,
								pAviEncPacker->avi_workmem,
								pAviEncPacker->index_write_buffer,
								pAviEncPacker->index_buffer_size,
								pAviEncPacker->print_flag);
	if(nRet<0) RETURN(STATUS_FAIL);
	AviPackerV3_SetErrHandler(pAviEncPacker->avi_workmem, avi_packer_err_handle);
	nRet = STATUS_OK;
Return:
	return nRet;
}

/****************************************************************************/
/*
 *	avi_enc_packer_start
 */
INT32U avi_enc_packer_start(AviEncPacker_t *pAviEncPacker)
{
	INT32S nRet; 
	INT32U bflag;

	if(pAviEncPacker == pAviEncPacker0) {
		bflag = C_AVI_ENCODE_PACKER0;
	} else {
		RETURN(STATUS_FAIL);
	}

	switch(pAviEncPara->source_type)
	{
	case SOURCE_TYPE_FS:
		avi_encode_set_avi_header(pAviEncPacker);
		nRet = AviPackerV3_Open(pAviEncPacker->avi_workmem,
								pAviEncPacker->file_handle,
								pAviEncPacker->p_avi_vid_stream_header,
								pAviEncPacker->bitmap_info_cblen,
								pAviEncPacker->p_avi_bitmap_info,
								pAviEncPacker->p_avi_aud_stream_header,
								pAviEncPacker->wave_info_cblen,
								pAviEncPacker->p_avi_wave_info);
		pfn_avi_encode_put_data = AviPackerV3_WriteData;
		break;
	case SOURCE_TYPE_USER_DEFINE:
		pfn_avi_encode_put_data = video_encode_frame_ready;
		break;
	}
	avi_encode_set_status(bflag);
	DEBUG_MSG(DBG_PRINT("a.AviPackerOpen[0x%x] = 0x%x\r\n", bflag, nRet));
Return:	
	return nRet;
}

/****************************************************************************/
/*
 *	avi_enc_packer_stop
 */
 INT32U avi_enc_packer_stop(AviEncPacker_t *pAviEncPacker)
{
	INT32S nRet;
	INT32U bflag;
	
	if(pAviEncPacker == pAviEncPacker0)
	{		
		bflag = C_AVI_ENCODE_PACKER0;
	}
	else
	{
		RETURN(STATUS_FAIL);
	}
	
	if(avi_encode_get_status() & bflag)
	{
		switch(pAviEncPara->source_type)
        {
		case SOURCE_TYPE_FS:
			video_encode_end(pAviEncPacker->avi_workmem);
           	nRet = AviPackerV3_Close(pAviEncPacker->avi_workmem);
           	avi_encode_close_file(pAviEncPacker);
        	break;		
        case SOURCE_TYPE_USER_DEFINE:
        	nRet = STATUS_OK;
        	break;
        } 
        
        if(nRet < 0) RETURN(STATUS_FAIL);
		avi_encode_clear_status(bflag);
		DEBUG_MSG(DBG_PRINT("c.AviPackerClose[0x%x] = 0x%x\r\n", bflag, nRet)); 
	}
	nRet = STATUS_OK;
Return:
	return nRet;
}

/*
static void avi_encode_sync_timer_isr(void)
{
	if(pAviEncPara->AviPackerCur->p_avi_wave_info)
	{
		if((pAviEncPara->tv - pAviEncPara->ta) < pAviEncPara->delta_ta)
		{
			pAviEncPara->tv += pAviEncPara->tick;
		}	 
	}
	else
	{
		pAviEncPara->tv += pAviEncPara->tick;
	}
	
	if((pAviEncPara->tv - pAviEncPara->Tv) > 0)
	{
		if(pAviEncPara->post_cnt == pAviEncPara->pend_cnt)
		{
			if(OSQPost(AVIEncodeApQ, (void*)MSG_AVI_ONE_FRAME_ENCODE) == OS_NO_ERR) {
				pAviEncPara->post_cnt++;
			}
		}
	}
}

void avi_encode_video_timer_start(void)
{
	INT32U temp, freq_hz;
	INT32U preload_value;
	
	pAviEncPara->ta = 0;
	pAviEncPara->tv = 0;
	pAviEncPara->Tv = 0;
	pAviEncPara->pend_cnt = 0;
	pAviEncPara->post_cnt = 0;

	if(AVI_AUDIO_RECORD_TIMER == ADC_AS_TIMER_A)
		preload_value = R_TIMERA_PRELOAD;	
	else if(AVI_AUDIO_RECORD_TIMER == ADC_AS_TIMER_B)
		preload_value = R_TIMERB_PRELOAD;	
	else if(AVI_AUDIO_RECORD_TIMER == ADC_AS_TIMER_C)
		preload_value = R_TIMERC_PRELOAD;
	else if(AVI_AUDIO_RECORD_TIMER == ADC_AS_TIMER_D)
		preload_value = R_TIMERD_PRELOAD;
	else if(AVI_AUDIO_RECORD_TIMER == ADC_AS_TIMER_E)
		preload_value = R_TIMERE_PRELOAD;
	else	//timerf 
		preload_value = R_TIMERF_PRELOAD;	

	if(pAviEncPara->AviPackerCur->p_avi_wave_info)
	{
		//temp = 0x10000 -((0x10000 - (R_TIMERE_PRELOAD & 0xFFFF)) * p_vid_dec_para->n);
		temp = (0x10000 - (preload_value & 0xFFFF)) * pAviEncPara->freq_div;
		freq_hz = MCLK/2/temp;
		if(MCLK%(2*temp))	freq_hz++;
	}
	else
		freq_hz = AVI_ENCODE_TIME_BASE;
		
	timer_freq_setup(AVI_ENCODE_VIDEO_TIMER, freq_hz, 0, avi_encode_sync_timer_isr);
}
*/

void avi_encode_video_timer_stop(void)
{
	timer_stop(AVI_ENCODE_VIDEO_TIMER);
}

void avi_encode_audio_timer_start(void)
{
	mic_sample_rate_set(AVI_AUDIO_RECORD_TIMER, pAviEncAudPara->audio_sample_rate);
}

void avi_encode_audio_timer_stop(void)
{
	mic_timer_stop(AVI_AUDIO_RECORD_TIMER);
}

// file handle
INT32S avi_encode_set_file_handle_and_caculate_free_size(AviEncPacker_t *pAviEncPacker, INT16S FileHandle)
{
	if(FileHandle < 0)
	{
		return STATUS_FAIL;
	}

  #if AVI_ENCODE_CAL_DISK_SIZE_EN
   	pAviEncPara->disk_free_size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE) - 3145728;	//3*1024*1024
   	pAviEncPara->record_total_size = 2*32*1024 + 16; //avi header + data is 32k align + index header
  #endif

    pAviEncPacker->file_handle = FileHandle;
   	return STATUS_OK;
}

INT16S avi_encode_close_file(AviEncPacker_t *pAviEncPacker)
{
	INT32S nRet;
	
	nRet = close(pAviEncPacker->file_handle);
	pAviEncPacker->file_handle = -1;
	return nRet;
}



INT32S avi_encode_set_avi_header(AviEncPacker_t *pAviEncPacker)
{
	INT16U sample_per_block, package_size;
	
	pAviEncPacker->p_avi_aud_stream_header = &avi_aud_stream_header;
	pAviEncPacker->p_avi_vid_stream_header = &avi_vid_stream_header;
	pAviEncPacker->p_avi_bitmap_info = &avi_bitmap_info;
	pAviEncPacker->p_avi_wave_info = &avi_wave_info;
	gp_memset((INT8S*)pAviEncPacker->p_avi_aud_stream_header, 0, sizeof(GP_AVI_AVISTREAMHEADER));
	gp_memset((INT8S*)pAviEncPacker->p_avi_vid_stream_header, 0, sizeof(GP_AVI_AVISTREAMHEADER));
	gp_memset((INT8S*)pAviEncPacker->p_avi_bitmap_info, 0, sizeof(GP_AVI_BITMAPINFO));
	gp_memset((INT8S*)pAviEncPacker->p_avi_wave_info, 0, sizeof(GP_AVI_PCMWAVEFORMAT));
	
	//audio
	avi_aud_stream_header.fccType[0] = 'a';
	avi_aud_stream_header.fccType[1] = 'u';
	avi_aud_stream_header.fccType[2] = 'd';
	avi_aud_stream_header.fccType[3] = 's';
	
	switch(pAviEncAudPara->audio_format) 
	{
	case WAVE_FORMAT_PCM:
		pAviEncPacker->wave_info_cblen = 16;
		avi_aud_stream_header.fccHandler[0] = 1;
		avi_aud_stream_header.fccHandler[1] = 0;
		avi_aud_stream_header.fccHandler[2] = 0;
		avi_aud_stream_header.fccHandler[3] = 0;
			
		avi_wave_info.wFormatTag = WAVE_FORMAT_PCM;
		avi_wave_info.nChannels = pAviEncAudPara->channel_no;	
		avi_wave_info.nSamplesPerSec =  pAviEncAudPara->audio_sample_rate;
		avi_wave_info.nAvgBytesPerSec =  pAviEncAudPara->channel_no * pAviEncAudPara->audio_sample_rate * 2; 
		avi_wave_info.nBlockAlign = pAviEncAudPara->channel_no * 2;
		avi_wave_info.wBitsPerSample = 16; //16bit
			
		avi_aud_stream_header.dwScale = avi_wave_info.nBlockAlign;
		avi_aud_stream_header.dwRate = avi_wave_info.nAvgBytesPerSec;
		avi_aud_stream_header.dwSampleSize = avi_wave_info.nBlockAlign;;	
		break;
		
	case WAVE_FORMAT_ADPCM:
		pAviEncPacker->wave_info_cblen = 50;
		avi_aud_stream_header.fccHandler[0] = 0;
		avi_aud_stream_header.fccHandler[1] = 0;
		avi_aud_stream_header.fccHandler[2] = 0;
		avi_aud_stream_header.fccHandler[3] = 0;

		package_size = 0x100;
		if(pAviEncAudPara->channel_no == 1)
			sample_per_block = 2 * package_size - 12;
		else if(pAviEncAudPara->channel_no == 2)
			sample_per_block = package_size - 12;
		else
			sample_per_block = 1;
		
		avi_wave_info.wFormatTag = WAVE_FORMAT_ADPCM;
		avi_wave_info.nChannels = pAviEncAudPara->channel_no;	
		avi_wave_info.nSamplesPerSec =  pAviEncAudPara->audio_sample_rate;
		avi_wave_info.nAvgBytesPerSec =  package_size * pAviEncAudPara->audio_sample_rate / sample_per_block; // = PackageSize * FrameSize / fs
		avi_wave_info.nBlockAlign = package_size; //PackageSize
		avi_wave_info.wBitsPerSample = 4; //4bit
		avi_wave_info.cbSize = 32;
		// extension ...
		avi_wave_info.ExtInfo[0] = 0x01F4;	avi_wave_info.ExtInfo[1] = 0x0007;	
		avi_wave_info.ExtInfo[2] = 0x0100;	avi_wave_info.ExtInfo[3] = 0x0000;
		avi_wave_info.ExtInfo[4] = 0x0200;	avi_wave_info.ExtInfo[5] = 0xFF00;
		avi_wave_info.ExtInfo[6] = 0x0000;	avi_wave_info.ExtInfo[7] = 0x0000;
		
		avi_wave_info.ExtInfo[8] =  0x00C0;	avi_wave_info.ExtInfo[9] =  0x0040;
		avi_wave_info.ExtInfo[10] = 0x00F0; avi_wave_info.ExtInfo[11] = 0x0000;
		avi_wave_info.ExtInfo[12] = 0x01CC; avi_wave_info.ExtInfo[13] = 0xFF30;
		avi_wave_info.ExtInfo[14] = 0x0188; avi_wave_info.ExtInfo[15] = 0xFF18;
		break;
		
	case WAVE_FORMAT_IMA_ADPCM:
		pAviEncPacker->wave_info_cblen = 20;
		avi_aud_stream_header.fccHandler[0] = 0;
		avi_aud_stream_header.fccHandler[1] = 0;
		avi_aud_stream_header.fccHandler[2] = 0;
		avi_aud_stream_header.fccHandler[3] = 0;
		
		package_size = 0x100;
		if(pAviEncAudPara->channel_no == 1)
			sample_per_block = 2 * package_size - 7;
		else if(pAviEncAudPara->channel_no == 2)
			sample_per_block = package_size - 7;
		else
			sample_per_block = 1;
		
		avi_wave_info.wFormatTag = WAVE_FORMAT_IMA_ADPCM;
		avi_wave_info.nChannels = pAviEncAudPara->channel_no;	
		avi_wave_info.nSamplesPerSec =  pAviEncAudPara->audio_sample_rate;
		avi_wave_info.nAvgBytesPerSec =  package_size * pAviEncAudPara->audio_sample_rate / sample_per_block;
		avi_wave_info.nBlockAlign = package_size;	//PackageSize
		avi_wave_info.wBitsPerSample = 4; //4bit
		avi_wave_info.cbSize = 2;
		// extension ...
		avi_wave_info.ExtInfo[0] = sample_per_block;
		break;
		
	default:
		pAviEncPacker->wave_info_cblen = 0;
		pAviEncPacker->p_avi_aud_stream_header = NULL; 
		pAviEncPacker->p_avi_wave_info = NULL;
	}
	
	avi_aud_stream_header.dwScale = avi_wave_info.nBlockAlign;
	avi_aud_stream_header.dwRate = avi_wave_info.nAvgBytesPerSec;
	avi_aud_stream_header.dwSampleSize = avi_wave_info.nBlockAlign;
	
	//video
	avi_vid_stream_header.fccType[0] = 'v';
	avi_vid_stream_header.fccType[1] = 'i';
	avi_vid_stream_header.fccType[2] = 'd';
	avi_vid_stream_header.fccType[3] = 's';
	avi_vid_stream_header.dwScale = pAviEncVidPara->dwScale;
	avi_vid_stream_header.dwRate = pAviEncVidPara->dwRate;
	avi_vid_stream_header.rcFrame.right = pAviEncVidPara->encode_width;
	avi_vid_stream_header.rcFrame.bottom = pAviEncVidPara->encode_height;
	switch(pAviEncVidPara->video_format)
	{
	case C_MJPG_FORMAT:
		avi_vid_stream_header.fccHandler[0] = 'm';
		avi_vid_stream_header.fccHandler[1] = 'j';
		avi_vid_stream_header.fccHandler[2] = 'p';
		avi_vid_stream_header.fccHandler[3] = 'g';
		
		avi_bitmap_info.biSize = pAviEncPacker->bitmap_info_cblen = 40;
		avi_bitmap_info.biWidth = pAviEncVidPara->encode_width;
		avi_bitmap_info.biHeight = pAviEncVidPara->encode_height;
		avi_bitmap_info.biBitCount = 24;
		avi_bitmap_info.biCompression[0] = 'M';
		avi_bitmap_info.biCompression[1] = 'J';
		avi_bitmap_info.biCompression[2] = 'P';
		avi_bitmap_info.biCompression[3] = 'G';
		avi_bitmap_info.biSizeImage = pAviEncVidPara->encode_width * pAviEncVidPara->encode_height * 3;
		break;
		
	case C_XVID_FORMAT:
		avi_vid_stream_header.fccHandler[0] = 'x';
		avi_vid_stream_header.fccHandler[1] = 'v';
		avi_vid_stream_header.fccHandler[2] = 'i';
		avi_vid_stream_header.fccHandler[3] = 'd';
	
		avi_bitmap_info.biSize = pAviEncPacker->bitmap_info_cblen = 68;
		avi_bitmap_info.biWidth = pAviEncVidPara->encode_width;
		avi_bitmap_info.biHeight = pAviEncVidPara->encode_height;
		avi_bitmap_info.biBitCount = 24;
		avi_bitmap_info.biCompression[0] = 'X';
		avi_bitmap_info.biCompression[1] = 'V';
		avi_bitmap_info.biCompression[2] = 'I';
		avi_bitmap_info.biCompression[3] = 'D';
		avi_bitmap_info.biSizeImage = pAviEncVidPara->encode_width * pAviEncVidPara->encode_height * 3;
		break; 
	} 
	
	return STATUS_OK;
}

void avi_encode_set_curworkmem(void *pAviEncPacker)
{
	 pAviEncPara->AviPackerCur = pAviEncPacker;
}

// status
void avi_encode_set_status(INT32U bit)
{
	pAviEncPara->avi_encode_status |= bit;
}  

void avi_encode_clear_status(INT32U bit)
{
	pAviEncPara->avi_encode_status &= ~bit;
}  

INT32S avi_encode_get_status(void)
{
    return pAviEncPara->avi_encode_status;
}

INT32U avi_encode_get_csi_frame(void)
{
	INT32U addr;
	
	addr =  pAviEncVidPara->csi_frame_addr[g_csi_index++];
	if(g_csi_index >= AVI_ENCODE_CSI_BUFFER_NO)
		g_csi_index = 0;  
	
	return addr;
}

INT32U avi_encode_get_scaler_frame(void)
{
	INT32U addr;
	
	addr = pAviEncVidPara->scaler_output_addr[g_scaler_index++];
	if(g_scaler_index >= AVI_ENCODE_SCALER_BUFFER_NO)
		g_scaler_index = 0;  
	
	return addr;
}

INT32U avi_encode_get_display_frame(void)
{
	INT32U addr, lock_cnt;
	
	lock_cnt = AVI_ENCODE_DISPALY_BUFFER_NO;
	
	do {
		g_display_index++;
		if (g_display_index >= AVI_ENCODE_DISPALY_BUFFER_NO) {
			g_display_index = 0;
		}
		if (pAviEncVidPara->display_output_addr[g_display_index] & MSG_SCALER_TASK_PREVIEW_LOCK) {
			addr = NULL;
		} else {
			addr = (INT32U) &pAviEncVidPara->display_output_addr[g_display_index];
			return addr;
		}
		lock_cnt--;
	} while (lock_cnt);
	return addr;
}

INT32U avi_encode_get_video_frame(void)
{
	INT32U i, i_bak, addr;
	INT32U free_buffer_count;

	free_buffer_count = 0;
	for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++)
	{
		if(pAviEncVidPara->video_encode_addr[i].is_used == 0)
		{
			free_buffer_count++;
			i_bak = i;
		}
	}

	if((free_buffer_count == 0) && time_stamp_buffer_size)
	{
		INT32U current_time;
		INT32U min_idx = time_stamp_buffer_size;
		INT32S min_delay = 0x10000, diff;
		INT32S prev, next;

		current_time = OSTimeGet() + 3;
		for(i = 0; i < (time_stamp_buffer_size + 1); i += 2) {
			if(i==0) prev = first_time;
			else prev = pAviEncVidPara->video_encode_addr[time_stamp_buffer[i-1]].buffer_time;

			if((i+1) >= time_stamp_buffer_size) next = current_time;
			else next = pAviEncVidPara->video_encode_addr[time_stamp_buffer[i+1]].buffer_time;

			diff = next - prev;
			if(diff < min_delay) {
				min_delay = diff;
				min_idx = i;
			}
		}

		if(min_idx < time_stamp_buffer_size) {
			free_buffer_count = 1;
			i_bak = time_stamp_buffer[min_idx];
			pAviEncVidPara->video_encode_addr[i_bak].is_used = 0;

			//drop the frame slelected before
			for(i=min_idx; i<time_stamp_buffer_size; i++) {
				if((i+1) < time_stamp_buffer_size) {
					time_stamp_buffer[i] = time_stamp_buffer[i+1];
				} else {
					time_stamp_buffer[i] = 0;
				}
			}
			time_stamp_buffer_size -= 1;
		}
	}

	if(free_buffer_count != 0)
	{
		free_buffer_count--;
		pAviEncVidPara->video_encode_addr[i_bak].is_used = 1;
		g_video_index = i_bak;
		addr = pAviEncVidPara->video_encode_addr[i_bak].buffer_addrs;
	}
	else
	{
		g_video_index = AVI_ENCODE_VIDEO_BUFFER_NO;
		addr = 0;
	}
	return addr;
}

static INT32S sensor_mem_alloc(void)
{
	INT32S i, buffer_size, nRet;
	
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	buffer_size = pAviEncVidPara->sensor_capture_width * pAviEncVidPara->sensor_capture_height << 1;
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	buffer_size = pAviEncVidPara->sensor_capture_width * SENSOR_FIFO_LINE << 1;
#endif
	for(i=0; i<AVI_ENCODE_CSI_BUFFER_NO; i++)
	{
		if(!pAviEncVidPara->csi_frame_addr[i])
			pAviEncVidPara->csi_frame_addr[i] = (INT32U) gp_malloc_align(buffer_size, 32);
		
		if(!pAviEncVidPara->csi_frame_addr[i]) {
			//mm_dump();
			RETURN(STATUS_FAIL);	
			}
		DEBUG_MSG(DBG_PRINT("sensor_frame_addr[%d] = 0x%x\r\n", i, pAviEncVidPara->csi_frame_addr[i]));
	}
//	mm_dump();
	nRet = STATUS_OK;	
Return:
	return nRet;
}

static INT32S scaler_mem_alloc(void)
{
	INT32S i, buffer_size, nRet;
	
#if VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FRAME_MODE
	if((AVI_ENCODE_DIGITAL_ZOOM_EN == 1) || pAviEncVidPara->scaler_flag)
	{
		buffer_size = pAviEncVidPara->encode_width * pAviEncVidPara->encode_height << 1;	
		for(i=0; i<AVI_ENCODE_SCALER_BUFFER_NO; i++)
		{
			if(!pAviEncVidPara->scaler_output_addr[i])
				pAviEncVidPara->scaler_output_addr[i] = (INT32U) gp_malloc_align(buffer_size, 32);
			
			if(!pAviEncVidPara->scaler_output_addr[i]) RETURN(STATUS_FAIL);
			DEBUG_MSG(DBG_PRINT("scaler_frame_addr[%d] = 0x%x\r\n", i, pAviEncVidPara->scaler_output_addr[i]));
		}
	}
#elif VIDEO_ENCODE_USE_MODE == SENSOR_BUF_FIFO_MODE
	if(pAviEncVidPara->scaler_flag)
	{
		nRet = pAviEncVidPara->encode_height /(pAviEncVidPara->sensor_capture_height / SENSOR_FIFO_LINE);
		nRet += 2; //+2 is to fix block line
		buffer_size = pAviEncVidPara->encode_width * nRet << 1; 
		for(i=0; i<AVI_ENCODE_SCALER_BUFFER_NO; i++)
		{
			if(!pAviEncVidPara->scaler_output_addr[i])
				pAviEncVidPara->scaler_output_addr[i] = (INT32U) gp_malloc_align(buffer_size, 32);
			
			if(!pAviEncVidPara->scaler_output_addr[i]) RETURN(STATUS_FAIL);
			DEBUG_MSG(DBG_PRINT("scaler_frame_addr[%d] = 0x%x\r\n", i, pAviEncVidPara->scaler_output_addr[i]));
		}
	}
#endif
	nRet = STATUS_OK;
Return:
	return nRet;
}

static INT32S display_mem_alloc(void)
{	
#if AVI_ENCODE_PREVIEW_DISPLAY_EN == 1
	INT32S nRet;
	INT32U buffer_size, i;
	
	if(pAviEncVidPara->dispaly_scaler_flag)
	{
		buffer_size = pAviEncVidPara->display_buffer_width * pAviEncVidPara->display_buffer_height << 1;
		for(i=0; i<AVI_ENCODE_DISPALY_BUFFER_NO; i++)
		{
			if(!pAviEncVidPara->display_output_addr[i])
				pAviEncVidPara->display_output_addr[i] = (INT32U) gp_malloc_align(buffer_size, 32);
			
			if(!pAviEncVidPara->display_output_addr[i]) RETURN(STATUS_FAIL);			
			DEBUG_MSG(DBG_PRINT("display_frame_addr[%d]= 0x%x\r\n", i, pAviEncVidPara->display_output_addr[i]));
		}
	}	
	nRet = STATUS_OK;
Return:
	return nRet;
#else
	return STATUS_OK;
#endif
}

static INT32S video_mem_alloc(void)
{
	INT32S i, nRet;
	INT32U buffer_addr;

#if AVI_ENCODE_VIDEO_ENCODE_EN == 1
	//buffer_size = pAviEncVidPara->encode_width * pAviEncVidPara->encode_height >> 1;

	buffer_addr = (INT32U) gp_malloc_align(AVI_ENCODE_VIDEO_BUFFER_NO * (AVI_ENCODE_VIDEO_BUFFER_SIZE + 16), 32);
	if(!buffer_addr) RETURN(STATUS_FAIL);

	write_buff_addr = buffer_addr;

	for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++)
	{
		pAviEncVidPara->video_encode_addr[i].buffer_addrs = buffer_addr + (AVI_ENCODE_VIDEO_BUFFER_SIZE+16)*i;
		pAviEncVidPara->video_encode_addr[i].buffer_idx = i;
		pAviEncVidPara->video_encode_addr[i].is_used = 0;
		pAviEncVidPara->video_encode_addr[i].msg_id = 0;
		pAviEncVidPara->video_encode_addr[i].buffer_len = 0;
		pAviEncVidPara->video_encode_addr[i].buffer_time = 0;
		pAviEncVidPara->video_encode_addr[i].ext = 0;
		pAviEncVidPara->video_encode_addr[i].jpeg_Q_value = 0;

		DEBUG_MSG(DBG_PRINT("jpeg_frame_addr[%d]   = 0x%x\r\n", i, pAviEncVidPara->video_encode_addr[i].buffer_addrs));
	}
#endif
	nRet = STATUS_OK;
Return:
	return nRet;
} 

INT32S AviPacker_mem_alloc(AviEncPacker_t *pAviEncPacker)
{
	INT32S nRet;
	
#if AVI_ENCODE_VIDEO_ENCODE_EN == 1		
	pAviEncPacker->index_buffer_size = IndexBuffer_Size;
	if(!pAviEncPacker->index_write_buffer)
		pAviEncPacker->index_write_buffer = (INT32U *)gp_malloc_align(IndexBuffer_Size, 4);
	//DEBUG_MSG(DBG_PRINT("s1"));
	if(!pAviEncPacker->index_write_buffer) RETURN(STATUS_FAIL);
	//DEBUG_MSG(DBG_PRINT("s2"));
	pAviEncPacker->avi_workmem = (void *)gp_malloc_align(AviPackerV3_GetWorkMemSize(), 4);
	if(!pAviEncPacker->avi_workmem ) RETURN(STATUS_FAIL);
	//DEBUG_MSG(DBG_PRINT("s3"));
	gp_memset((INT8S*)pAviEncPacker->avi_workmem, 0x00, AviPackerV3_GetWorkMemSize());
	DEBUG_MSG(DBG_PRINT("index_write_buffer= 0x%x\r\n", pAviEncPacker->index_write_buffer));
#endif
	nRet = STATUS_OK;
Return:
	return nRet;
} 

static void sensor_mem_free(void) 
{
	INT32S i;
	for(i=0; i<AVI_ENCODE_CSI_BUFFER_NO; i++)
	{
		if(pAviEncVidPara->csi_frame_addr[i])
			gp_free((void*)pAviEncVidPara->csi_frame_addr[i]);
		
		pAviEncVidPara->csi_frame_addr[i] = 0;
	}
}

static void scaler_mem_free(void) 
{
	INT32S i;
	for(i=0; i<AVI_ENCODE_SCALER_BUFFER_NO; i++)
	{
		if(pAviEncVidPara->scaler_output_addr[i])
			gp_free((void*)pAviEncVidPara->scaler_output_addr[i]);
		
		pAviEncVidPara->scaler_output_addr[i] = 0;
	}
}

static void display_mem_free(void) 
{
	INT32S i;
	for(i=0; i<AVI_ENCODE_DISPALY_BUFFER_NO; i++)
	{
		if(pAviEncVidPara->display_output_addr[i])
			gp_free((void*)(pAviEncVidPara->display_output_addr[i] & 0xFFFFFF));	
		
		pAviEncVidPara->display_output_addr[i] = 0;
	}
}

static void video_mem_free(void) 
{
	INT32S i;

	if(pAviEncVidPara->video_encode_addr[0].buffer_addrs)
	{
		gp_free((void*)pAviEncVidPara->video_encode_addr[0].buffer_addrs);
	}

	write_buff_addr = 0;
	for(i=0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++)
	{
		pAviEncVidPara->video_encode_addr[i].buffer_addrs = 0;
		pAviEncVidPara->video_encode_addr[i].is_used = 0;
	}
}
/*
static void AviPacker_mem_free(AviEncPacker_t *pAviEncPacker)
{
	if(pAviEncPacker->index_write_buffer)
		gp_free((void*)pAviEncPacker->index_write_buffer);

	if(pAviEncPacker->avi_workmem)
		gp_free((void*)pAviEncPacker->avi_workmem);	

	pAviEncPacker->index_write_buffer = 0;
	pAviEncPacker->avi_workmem = 0;
}
*/
INT32U write_buff_addr;
INT32S avi_encode_memory_alloc(void)
{
	INT32S nRet;

	g_csi_index = 0;
	g_scaler_index = 0;
	g_display_index = 0;
	g_video_index = 0;
	g_pcm_index = 0;
	write_buff_addr = 0;
	if(sensor_mem_alloc() < 0) RETURN(STATUS_FAIL);
	if(scaler_mem_alloc() < 0) RETURN(STATUS_FAIL); 
	if(display_mem_alloc() < 0) RETURN(STATUS_FAIL);     
	if(video_mem_alloc() < 0) RETURN(STATUS_FAIL);
	//if(AviPacker_mem_alloc(pAviEncPacker0) < 0) {
	//	RETURN(STATUS_FAIL);
	//}

#if AVI_ENCODE_FAST_SWITCH_EN == 1	
	//if(AviPacker_mem_alloc(pAviEncPacker1) < 0) RETURN(STATUS_FAIL);
#endif
	nRet = STATUS_OK;
Return:	
	return nRet;
}

void avi_encode_memory_free(void)
{
	sensor_mem_free();
	scaler_mem_free();
	display_mem_free();
	video_mem_free();
	//AviPacker_mem_free(pAviEncPacker0);
#if AVI_ENCODE_FAST_SWITCH_EN == 1		
	//AviPacker_mem_free(pAviEncPacker1);
#endif
}

#if MPEG4_ENCODE_ENABLE == 1
INT32S avi_encode_mpeg4_malloc(INT16U width, INT16U height)
{
	INT32S size, nRet;
	
	size = 16*width*2*3/4;
	pAviEncPara->isram_addr = (INT32U)gp_iram_malloc_align(size, 32);
	if(!pAviEncPara->isram_addr) RETURN(STATUS_FAIL);
	
	size = width*height*2;
	pAviEncPara->write_refer_addr = (INT32U)gp_malloc_align(size, 32);
	if(!pAviEncPara->write_refer_addr) RETURN(STATUS_FAIL);
	
	size = width*height*2;	
	pAviEncPara->reference_addr = (INT32U)gp_malloc_align(size, 32);
	if(!pAviEncPara->reference_addr) RETURN(STATUS_FAIL);		
	
	nRet = STATUS_OK;
Return:		
	return nRet;
}

void avi_encode_mpeg4_free(void)
{
	gp_free((void*)pAviEncPara->isram_addr);
	pAviEncPara->isram_addr = 0;
	
	gp_free((void*)pAviEncPara->write_refer_addr);
	pAviEncPara->write_refer_addr = 0;
	
	gp_free((void*)pAviEncPara->reference_addr);
	pAviEncPara->reference_addr = 0;
}
#endif //MPEG4_ENCODE_ENABLE

// format
void avi_encode_set_sensor_format(INT32U format)
{
	if(format == BITMAP_YUYV)
    	pAviEncVidPara->sensor_output_format = C_SCALER_CTRL_IN_YUYV;
    else if(format ==  BITMAP_RGRB)	
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_IN_RGBG;
    else
    	pAviEncVidPara->sensor_output_format = C_SCALER_CTRL_IN_YUYV;
} 

void avi_encode_set_display_format(INT32U format)
{
	if(format == IMAGE_OUTPUT_FORMAT_RGB565)
		pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_RGB565;
    else if(format == IMAGE_OUTPUT_FORMAT_RGBG)
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_RGBG;
	else if(format == IMAGE_OUTPUT_FORMAT_YUYV)
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_YUYV;
	else if(format == IMAGE_OUTPUT_FORMAT_GRGB)
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_GRGB;
    else if(format == IMAGE_OUTPUT_FORMAT_UYVY)
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_UYVY;
    else
    	pAviEncVidPara->display_output_format = C_SCALER_CTRL_OUT_YUYV;	
}

void avi_encode_set_display_scaler(void)
{
	if((pAviEncVidPara->encode_width != pAviEncVidPara->display_buffer_width)||
		(pAviEncVidPara->encode_height != pAviEncVidPara->display_buffer_height) || 
		(pAviEncVidPara->display_output_format != C_SCALER_CTRL_OUT_YUYV))
		pAviEncVidPara->dispaly_scaler_flag = 0;
	else
		pAviEncVidPara->dispaly_scaler_flag = 0;

	if((pAviEncVidPara->sensor_capture_width != pAviEncVidPara->encode_width) || 
		(pAviEncVidPara->sensor_capture_height != pAviEncVidPara->encode_height))
		pAviEncVidPara->scaler_flag = 0;
	else
		pAviEncVidPara->scaler_flag = 0;
}

INT32S avi_encode_set_jpeg_quality(INT32U addr, INT8U quality_value)
{
	INT8U *src;
	INT32U header_size;

	src = jpeg_avi_header;
	jpeg_header_quantization_table_calculate(0, quality_value, src + 20);
	jpeg_header_quantization_table_calculate(1, quality_value, src + 89);
    
    pAviEncVidPara->quality_value = quality_value; 
    header_size = sizeof(jpeg_avi_header);

	//copy jpeg header to buffer
	gp_memcpy((INT8S*)addr, (INT8S*)src, header_size);

	//set jpeg width and height to buffer
	src = (INT8U *)addr;
	*(src+0x9E) = (pAviEncVidPara->encode_height >> 8) & 0xFF;
    *(src+0x9F) = (pAviEncVidPara->encode_height & 0xFF);
    *(src+0xA0) = (pAviEncVidPara->encode_width >> 8) & 0xFF;
    *(src+0xA1) = (pAviEncVidPara->encode_width & 0xFF); 
    
    //if (pAviEncVidPara->encode_width == 1280) {
	//	*(src+0xA4) = 0x22; 	// YUV420
  	//} else {
  		*(src+0xA4) = 0x21; 	// YUV422
  	//}

	return header_size;
}

INT32S avi_encode_set_mp4_resolution(INT16U encode_width, INT16U encode_height)
{
	INT8U *src;
	INT32U i, header_size, video_frame;
	
	if(encode_width == 640 && encode_height == 480)
		src = mpeg4_header_vga;
 	else if(encode_width == 320 && encode_height == 240)
 		src = mpeg4_header_qvga;
 	else if(encode_width == 160 && encode_height == 120)
 		src = mpeg4_header_qqvga;
 	else if(encode_width == 352 && encode_height == 288)
 		src = mpeg4_header_cif;
 	else if(encode_width == 176 && encode_height == 144)
 		src = mpeg4_header_qcif;
 	else if(encode_width == 128 && encode_height == 96)
 		src = mpeg4_header_sqcif;
 	else
 	{
 		DEBUG_MSG(DBG_PRINT("mpeg4 header fail!!!\r\n"));
 		return STATUS_FAIL;	
 	}
 	
	header_size = sizeof(mpeg4_header_vga);
	for(i = 0; i<AVI_ENCODE_VIDEO_BUFFER_NO; i++)
	{	
	   	video_frame = avi_encode_get_video_frame();
	  	gp_memcpy((INT8S*)video_frame, (INT8S*)src, header_size);
	}
	return header_size;
}

BOOLEAN avi_encode_is_pause(void)
{
	INT32U status;
	status = pAviEncPara->avi_encode_status & C_AVI_ENCODE_PAUSE;
	if(!status)
		return FALSE;
	
	return TRUE;
}

// check disk free size
INT32S avi_encode_disk_size_is_enough(INT32S cb_write_size)
{
    INT32S nRet;
#if AVI_ENCODE_CAL_DISK_SIZE_EN	
	INT32U temp;
	
	INT64U disk_free_size;
	
	temp = pAviEncPara->record_total_size;
	disk_free_size = pAviEncPara->disk_free_size;
	temp += cb_write_size;
	if(temp >= AVI_FILE_MAX_RECORD_SIZE) RETURN(2);
	if(temp >= disk_free_size) RETURN(FALSE);
	pAviEncPara->record_total_size = temp;
#endif

	nRet = TRUE;
#if AVI_ENCODE_CAL_DISK_SIZE_EN	
Return:
#endif
	return nRet;
}

INT8U capture_flag=0;
// csi frame mode switch buffer
void avi_encode_switch_csi_frame_buffer(void)
{
  #if AVI_ENCODE_CSI_BUFFER_NO == 1
	if (capture_flag) {
		*P_CSI_TG_FBSADDR = 0x40000000;
	} else {
		*P_CSI_TG_FBSADDR = pAviEncVidPara->csi_frame_addr[0];
	}
	OSQPost(vid_enc_task_q, (void *) pAviEncVidPara->csi_frame_addr[0]);
  #else
	INT32U addr, addr1;
	INT8U  err;

	addr = (INT32U) OSQAccept(cmos_frame_q, &err);
	if((addr == NULL) || (err != OS_NO_ERR)) {
		*P_CSI_TG_FBSADDR = 0x40000000;
	} else {
		addr1 = *P_CSI_TG_FBSADDR;
		*P_CSI_TG_FBSADDR = addr;
	}
	OSQPost(vid_enc_task_q, (void *) addr1);
  #endif
}

// csi fifo mode switch buffer
void vid_enc_csi_fifo_end(INT8U flag, INT8U type)
{
	INT8U err;
	INT32U ready_frame, next_frame;
	
	next_frame = (INT32U) OSQAccept(cmos_frame_q, &err);
	if (err != OS_NO_ERR || !next_frame) {
		next_frame = 0x40000000;
	}
	if (flag & 0x1) {
		ready_frame = *P_CSI_TG_FBSADDR;
		*P_CSI_TG_FBSADDR = next_frame;
	} else {
		ready_frame = *P_CSI_TG_FBSADDR_B;
		*P_CSI_TG_FBSADDR_B = next_frame;
	}

	if(ready_frame != 0x40000000) {
		if (type) {
			err = OSQPost(vid_enc_task_q, (void *) (ready_frame | 0x80000000));
		} else {
			err = OSQPost(vid_enc_task_q, (void *) ready_frame);
		}

		if(err != OS_NO_ERR) {
			OSQPost(cmos_frame_q, (void *)ready_frame);
		}
	}
}

INT32S scaler_zoom_once(INT32U scaler_mode, FP32 bScalerFactor, 
						INT32U InputFormat, INT32U OutputFormat,
						INT16U input_x, INT16U input_y, 
						INT16U output_x, INT16U output_y,
						INT16U output_buffer_x, INT16U output_buffer_y, 
						INT32U scaler_input_y, INT32U scaler_input_u, INT32U scaler_input_v, 
						INT32U scaler_output_y, INT32U scaler_output_u, INT32U scaler_output_v)
{
	FP32    zoom_temp, ratio;
	INT32S  scaler_status;
	INT32U  temp_x, temp_y;
	
	scaler_lock();	
	
  	// Initiate Scaler
  	scaler_init();
	switch(scaler_mode) 
	{
	case C_SCALER_FULL_SCREEN:	
		scaler_image_pixels_set(input_x, input_y, output_buffer_x, output_buffer_y);	
		break;
		
	case C_SCALER_FIT_RATIO:	
		temp_x = (input_x<<16) / output_x;
		temp_y = (input_y<<16) / output_y;
		scaler_input_pixels_set(input_x, input_y);
		scaler_input_visible_pixels_set(input_x, input_y);
		scaler_output_pixels_set(temp_x, temp_y, output_buffer_x, output_buffer_y);
		scaler_input_offset_set(0, 0);
		break;
	
	case C_NO_SCALER_FIT_BUFFER:
		if((input_x <= output_x) && (input_y <= output_y))
		{
			temp_x = temp_y = (1<<16);
		}
		else
		{
			if (output_y*input_x > output_x*input_y) 
			{
	      		temp_x = temp_y = (input_x<<16) / output_x;
	      		output_y = (output_y<<16) / temp_x;
	      	} 
	      	else 
	      	{
	      		temp_x = temp_y = (input_y<<16) / output_y;
	      		output_x = (input_x<<16) / temp_x;
	      	}
		}
		scaler_input_pixels_set(input_x, input_y);
		scaler_input_visible_pixels_set(input_x, input_y);
		scaler_output_pixels_set(temp_x, temp_y, output_buffer_x, output_buffer_y);
		scaler_input_offset_set(0, 0);
		break;
		
	case C_SCALER_FIT_BUFFER:
		if (output_y*input_x > output_x*input_y) 
		{
      		temp_x = (input_x<<16) / output_x;
      		output_y = (output_y<<16) / temp_x;
      	} 
      	else 
      	{
      		temp_x = (input_y<<16) / output_y;
      		output_x = (input_x<<16) / temp_x;
      	}
      	scaler_input_pixels_set(input_x, input_y);
		scaler_input_visible_pixels_set(input_x, input_y);
      	scaler_output_pixels_set(temp_x, temp_x, output_buffer_x, output_buffer_y);
      	scaler_input_offset_set(0, 0);
		break;
		
	case C_SCALER_ZOOM_FIT_BUFFER:
		scaler_input_pixels_set(input_x, input_y);
		scaler_input_visible_pixels_set(input_x, input_y);	
		ratio = output_x/input_x;
		zoom_temp = 65536 / (ratio * bScalerFactor);
		scaler_output_pixels_set((int)zoom_temp, (int)zoom_temp, output_x, output_y);
		zoom_temp = 1 - (1 / bScalerFactor);
		scaler_input_offset_set((int)((float)(output_x/2)*zoom_temp)<<16, (int)((float)(output_y/2)*zoom_temp)<<16);
		break;	
		
	default:
		return -1;
	}
	
	scaler_input_addr_set(scaler_input_y, scaler_input_u, scaler_input_v);
   	scaler_output_addr_set(scaler_output_y, scaler_output_u, scaler_output_v);
   	scaler_fifo_line_set(C_SCALER_CTRL_FIFO_DISABLE);
	scaler_input_format_set(InputFormat);
	scaler_output_format_set(OutputFormat);
	scaler_out_of_boundary_color_set(0x008080);

	while(1)
	{	
		scaler_status = scaler_wait_idle();
		if (scaler_status == C_SCALER_STATUS_STOP) 
			scaler_start();
		else if (scaler_status & C_SCALER_STATUS_DONE)
		{
			scaler_stop();
			break;
		}
		else if (scaler_status & C_SCALER_STATUS_TIMEOUT)
		{
			DEBUG_MSG(DBG_PRINT("Scaler Timeout failed to finish its job\r\n"));
		} 
		else if(scaler_status & C_SCALER_STATUS_INIT_ERR) 
		{
			DEBUG_MSG(DBG_PRINT("Scaler INIT ERR failed to finish its job\r\n"));
		} 
		else if (scaler_status & C_SCALER_STATUS_INPUT_EMPTY) 
		{
  			scaler_restart();
  		}
  		else 
  		{
	  		DEBUG_MSG(DBG_PRINT("Un-handled Scaler status!\r\n"));
	  	}
  	}
	
	scaler_unlock();
			
	return scaler_status;
}

INT32S scale_up_and_encode(INT32U sensor_input_addr, INT32U scaler_output_fifo, INT32U jpeg_encode_width, INT32U jpeg_encode_height, INT32U jpeg_output_addr)
{
	INT32S scaler_status, jpeg_status;
	INT32U padding_width;
	INT32U padding_height;
	INT32U fifo_size;
	INT32U fifo_encode_size;
	INT32U total_encode_size;
	INT32U restart_index;
	INT32U jpeg_input_fifo_cnt;

	padding_width = (jpeg_encode_width + 15) & 0xFFF0;
	padding_height = (jpeg_encode_height + 7) & 0xFFF8;

	// Scaler output fifo mode
	scaler_init();
	scaler_input_pixels_set(pAviEncVidPara->sensor_capture_width, pAviEncVidPara->sensor_capture_height);							// 1~8000 pixels, including the padding pixels
	scaler_input_visible_pixels_set(pAviEncVidPara->sensor_capture_width, pAviEncVidPara->sensor_capture_height);					// 1~8000 pixels, not including the padding pixels
	scaler_input_addr_set(sensor_input_addr, NULL, NULL);
	scaler_input_format_set(C_SCALER_CTRL_IN_YUYV);
	scaler_output_pixels_set((pAviEncVidPara->sensor_capture_width<<16)/jpeg_encode_width, (pAviEncVidPara->sensor_capture_height<<16)/jpeg_encode_height, padding_width, jpeg_encode_height);
	scaler_output_addr_set(scaler_output_fifo, NULL, NULL);
	scaler_output_format_set(C_SCALER_CTRL_OUT_YUYV);
	scaler_input_fifo_line_set(C_SCALER_CTRL_IN_FIFO_DISABLE);	// C_SCALER_CTRL_IN_FIFO_DISABLE/C_SCALER_CTRL_IN_FIFO_16LINE/C_SCALER_CTRL_IN_FIFO_32LINE/C_SCALER_CTRL_IN_FIFO_64LINE/C_SCALER_CTRL_IN_FIFO_128LINE/C_SCALER_CTRL_IN_FIFO_256LINE
	scaler_output_fifo_line_set(C_SCALER_CTRL_OUT_FIFO_16LINE);	// C_SCALER_CTRL_OUT_FIFO_DISABLE/C_SCALER_CTRL_OUT_FIFO_16LINE/C_SCALER_CTRL_OUT_FIFO_32LINE/C_SCALER_CTRL_OUT_FIFO_64LINE
	scaler_out_of_boundary_mode_set(0);							// mode: 0=Use the boundary data of the input picture, 1=Use Use color defined in scaler_out_of_boundary_color_set()

	// Scaler start, restart and stop function APIs
	scaler_start();


	// JPEG input fifo mode
	jpeg_encode_init();
	gplib_jpeg_default_huffman_table_load();

	if (padding_height > 32) {
		jpeg_encode_input_size_set(padding_width, 16*2);		// Width and height(A/B fifo) of the image that will be compressed
	} else {
		jpeg_encode_input_size_set(padding_width, padding_height);
	}
	jpeg_encode_input_format_set(C_JPEG_FORMAT_YUYV);			// format: C_JPEG_FORMAT_YUV_SEPARATE or C_JPEG_FORMAT_YUYV
	jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);		// encode_mode: C_JPG_CTRL_YUV422 or C_JPG_CTRL_YUV420(only valid for C_JPEG_FORMAT_YUV_SEPARATE format)
	jpeg_encode_output_addr_set(jpeg_output_addr);				// The address that VLC(variable length coded) data will be output

	total_encode_size = 0;
	fifo_size = ((jpeg_encode_width+15) & 0xFFF0) * 16 * 2;
	restart_index = 0;
	jpeg_input_fifo_cnt = 0;

	while (1) {
		scaler_status = scaler_wait_idle();
		if (scaler_status & C_SCALER_STATUS_DONE) {
		  	// Wait until scaler finish its job
		  	while (1) {
		  		jpeg_status = jpeg_encode_status_query(1);
		  		if (jpeg_status & C_JPG_STATUS_ENCODE_DONE) {
		  			fifo_encode_size = jpeg_encode_vlc_cnt_get();
		  			cache_invalid_range(jpeg_output_addr+total_encode_size, fifo_encode_size);
					if (padding_height > 32) {
						// Replace 0xFFD9 by 0xFFD0~0xFFD7
						if (!(fifo_encode_size & 0xF)) {
							*((INT8U *) (jpeg_output_addr+total_encode_size+fifo_encode_size-1)) = 0xD0 + restart_index;
						} else {
							INT32U temp;
							INT8U *ptr;

							fifo_encode_size -= 2;
							ptr = (INT8U *) (jpeg_output_addr+total_encode_size+fifo_encode_size);
							temp = fifo_encode_size & 0xF;
							if (temp == 0xF) {
								*ptr++ = 0x55;
								temp = 0;
								fifo_encode_size++;
							}
							while (temp < 14) {
								*ptr++ = 0x55;
								temp++;
								fifo_encode_size++;
							}
							*ptr++ = 0xFF;
							*ptr++ = 0xD0 + restart_index;
							fifo_encode_size += 2;
						}
						restart_index++;
						if (restart_index >= 8) {
							restart_index = 0;
						}
					}
					// TBD: Save VLC of this FIFO to SD card
					total_encode_size += fifo_encode_size;
					write(pAviEncPara->AviPackerCur->file_handle, jpeg_output_addr, total_encode_size);
					total_encode_size = 0;

					if (padding_height > 32) {
						padding_height -= 32;

						jpeg_encode_init();
						if (padding_height > 32) {
							jpeg_encode_input_size_set(padding_width, 16*2);		// Width and height(A/B fifo) of the image that will be compressed
						} else {
							jpeg_encode_input_size_set(padding_width, padding_height);
						}
						jpeg_encode_input_format_set(C_JPEG_FORMAT_YUYV);			// format: C_JPEG_FORMAT_YUV_SEPARATE or C_JPEG_FORMAT_YUYV
						jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);		// encode_mode: C_JPG_CTRL_YUV422 or C_JPG_CTRL_YUV420(only valid for C_JPEG_FORMAT_YUV_SEPARATE format)
						jpeg_encode_output_addr_set(jpeg_output_addr+total_encode_size);	// The address that VLC(variable length coded) data will be output

						if (jpeg_encode_on_the_fly_start(scaler_output_fifo, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					} else {
						break;
					}

				} else if (jpeg_status & (C_JPG_STATUS_INPUT_EMPTY|C_JPG_STATUS_STOP)) {
					if (!(jpeg_input_fifo_cnt & 0x1)) {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					} else {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo+fifo_size, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					}
			  	}
		  	}
			break;
		}

		if (scaler_status & C_SCALER_STATUS_OUTPUT_FULL) {
			while (1) {
			  	// Start JPEG to handle the full output FIFO now
		  		jpeg_status = jpeg_encode_status_query(1);
		  		if (jpeg_status & C_JPG_STATUS_ENCODE_DONE) {
		  			fifo_encode_size = jpeg_encode_vlc_cnt_get();
		  			cache_invalid_range(jpeg_output_addr+total_encode_size, fifo_encode_size);
					// Replace 0xFFD9 by 0xFFD0~0xFFD7
					if (!(fifo_encode_size & 0xF)) {
						*((INT8U *) (jpeg_output_addr+total_encode_size+fifo_encode_size-1)) = 0xD0 + restart_index;
					} else {
						INT32U temp;
						INT8U *ptr;

						fifo_encode_size -= 2;
						ptr = (INT8U *) (jpeg_output_addr+total_encode_size+fifo_encode_size);
						temp = fifo_encode_size & 0xF;
						if (temp == 0xF) {
							*ptr++ = 0x55;
							temp = 0;
							fifo_encode_size++;
						}
						while (temp < 14) {
							*ptr++ = 0x55;
							temp++;
							fifo_encode_size++;
						}
						*ptr++ = 0xFF;
						*ptr++ = 0xD0 + restart_index;
						fifo_encode_size += 2;
					}
					restart_index++;
					if (restart_index >= 8) {
						restart_index = 0;
					}
					// TBD: Save VLC of this FIFO to SD card
					total_encode_size += fifo_encode_size;
					write(pAviEncPara->AviPackerCur->file_handle, jpeg_output_addr, total_encode_size);
					total_encode_size = 0;

					if (padding_height > 32) {
						padding_height -= 32;

						jpeg_encode_init();
						if (padding_height > 32) {
							jpeg_encode_input_size_set(padding_width, 16*2);		// Width and height(A/B fifo) of the image that will be compressed
						} else {
							jpeg_encode_input_size_set(padding_width, padding_height);
						}
						jpeg_encode_input_format_set(C_JPEG_FORMAT_YUYV);			// format: C_JPEG_FORMAT_YUV_SEPARATE or C_JPEG_FORMAT_YUYV
						jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);		// encode_mode: C_JPG_CTRL_YUV422 or C_JPG_CTRL_YUV420(only valid for C_JPEG_FORMAT_YUV_SEPARATE format)
						jpeg_encode_output_addr_set(jpeg_output_addr+total_encode_size);	// The address that VLC(variable length coded) data will be output

					} else {
						jpeg_encode_stop();
						scaler_stop();
						return total_encode_size;
					}

					jpeg_input_fifo_cnt = 0;

				} else if ((jpeg_status & (C_JPG_STATUS_INPUT_EMPTY|C_JPG_STATUS_STOP)) && (jpeg_input_fifo_cnt<2)) {
			  		// Now restart Scaler to output to next FIFO
			  		if (scaler_restart()) {
			  			jpeg_encode_stop();
						scaler_stop();
						return 0;
				  	}

					if (!(jpeg_input_fifo_cnt & 0x1)) {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					} else {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo+fifo_size, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					}
					jpeg_input_fifo_cnt++;
					break;
			  	}
			}
		}
	}

	jpeg_encode_stop();
	scaler_stop();

	return total_encode_size;
}

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
INT32S scale_up_and_encode_fifo(INT32U sensor_input_addr, INT32U input_width, INT32U input_height, INT32U scaler_output_fifo, INT32U jpeg_encode_width, INT32U jpeg_encode_height, INT32U jpeg_output_addr, INT32U restart_idx)
{
	INT32S scaler_status, jpeg_status;
	INT32U padding_width;
	INT32U padding_height;
	INT32U fifo_size;
	INT32U fifo_encode_size;
	INT32U jpeg_input_fifo_cnt;

	padding_width = (jpeg_encode_width + 15) & 0xFFF0;
	padding_height = (jpeg_encode_height + 7) & 0xFFF8;

	// Scaler output fifo mode
	scaler_init();
	scaler_input_pixels_set(input_width, input_height);							// 1~8000 pixels, including the padding pixels
	scaler_input_visible_pixels_set(input_width, input_height);					// 1~8000 pixels, not including the padding pixels
	scaler_input_addr_set(sensor_input_addr, NULL, NULL);
	scaler_input_format_set(C_SCALER_CTRL_IN_YUYV);
	scaler_output_pixels_set((input_width<<16)/jpeg_encode_width, (input_height<<16)/jpeg_encode_height, padding_width, jpeg_encode_height);
	scaler_output_addr_set(scaler_output_fifo, NULL, NULL);
	scaler_output_format_set(C_SCALER_CTRL_OUT_YUYV);
	scaler_input_fifo_line_set(C_SCALER_CTRL_IN_FIFO_DISABLE);	// C_SCALER_CTRL_IN_FIFO_DISABLE/C_SCALER_CTRL_IN_FIFO_16LINE/C_SCALER_CTRL_IN_FIFO_32LINE/C_SCALER_CTRL_IN_FIFO_64LINE/C_SCALER_CTRL_IN_FIFO_128LINE/C_SCALER_CTRL_IN_FIFO_256LINE

	//if(jpeg_encode_height >= 128)
	//	scaler_output_fifo_line_set(C_SCALER_CTRL_OUT_FIFO_64LINE);	// C_SCALER_CTRL_OUT_FIFO_DISABLE/C_SCALER_CTRL_OUT_FIFO_16LINE/C_SCALER_CTRL_OUT_FIFO_32LINE/C_SCALER_CTRL_OUT_FIFO_64LINE
	//else
	//	scaler_output_fifo_line_set(C_SCALER_CTRL_OUT_FIFO_DISABLE);	// C_SCALER_CTRL_OUT_FIFO_DISABLE/C_SCALER_CTRL_OUT_FIFO_16LINE/C_SCALER_CTRL_OUT_FIFO_32LINE/C_SCALER_CTRL_OUT_FIFO_64LINE

	scaler_output_fifo_line_set(C_SCALER_CTRL_OUT_FIFO_16LINE);	// C_SCALER_CTRL_OUT_FIFO_DISABLE/C_SCALER_CTRL_OUT_FIFO_16LINE/C_SCALER_CTRL_OUT_FIFO_32LINE/C_SCALER_CTRL_OUT_FIFO_64LINE
	scaler_out_of_boundary_mode_set(0);							// mode: 0=Use the boundary data of the input picture, 1=Use Use color defined in scaler_out_of_boundary_color_set()

	// Scaler start, restart and stop function APIs
	scaler_start();

	// JPEG input fifo mode
	jpeg_encode_init();
	gplib_jpeg_default_huffman_table_load();

	jpeg_encode_input_size_set(padding_width, padding_height);
	jpeg_encode_input_format_set(C_JPEG_FORMAT_YUYV);			// format: C_JPEG_FORMAT_YUV_SEPARATE or C_JPEG_FORMAT_YUYV
	jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);		// encode_mode: C_JPG_CTRL_YUV422 or C_JPG_CTRL_YUV420(only valid for C_JPEG_FORMAT_YUV_SEPARATE format)
	jpeg_encode_output_addr_set(jpeg_output_addr);				// The address that VLC(variable length coded) data will be output

	fifo_size = ((jpeg_encode_width+15) & 0xFFF0) * 16 * 2;
	jpeg_input_fifo_cnt = 0;

	while (1) {
		scaler_status = scaler_wait_idle();
		if (scaler_status & C_SCALER_STATUS_DONE) {
		  	// Wait until scaler finish its job
		  	while (1) {
		  		jpeg_status = jpeg_encode_status_query(1);
		  		if (jpeg_status & C_JPG_STATUS_ENCODE_DONE) {
		  			fifo_encode_size = jpeg_encode_vlc_cnt_get();
		  			cache_invalid_range(jpeg_output_addr, fifo_encode_size);

					// Replace 0xFFD9 by 0xFFD0~0xFFD7
					if (!(fifo_encode_size & 0xF)) {
						*((INT8U *) (jpeg_output_addr+fifo_encode_size-1)) = 0xD0 + restart_idx;
					} else {
						INT32U temp;
						INT8U *ptr;

						fifo_encode_size -= 2;
						ptr = (INT8U *) (jpeg_output_addr+fifo_encode_size);
						temp = fifo_encode_size & 0xF;
						if (temp == 0xF) {
							*ptr++ = 0x55;
							temp = 0;
							fifo_encode_size++;
						}
						while (temp < 14) {
							*ptr++ = 0x55;
							temp++;
							fifo_encode_size++;
						}
						*ptr++ = 0xFF;
						*ptr++ = 0xD0 + restart_idx;
						fifo_encode_size += 2;
					}
					break;
				} else if (jpeg_status & (C_JPG_STATUS_INPUT_EMPTY|C_JPG_STATUS_STOP)) {
					if (!(jpeg_input_fifo_cnt & 0x1)) {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					} else {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo+fifo_size, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					}
			  	}
		  	}
			break;
		}

		if (scaler_status & C_SCALER_STATUS_OUTPUT_FULL) {
			while (1) {
			  	// Start JPEG to handle the full output FIFO now
		  		jpeg_status = jpeg_encode_status_query(1);
		  		if (jpeg_status & C_JPG_STATUS_ENCODE_DONE) {

					DBG_PRINT("scale_up_and_encode error\r\n");
					while(1);

				} else if ((jpeg_status & (C_JPG_STATUS_INPUT_EMPTY|C_JPG_STATUS_STOP)) && (jpeg_input_fifo_cnt<2)) {
			  		// Now restart Scaler to output to next FIFO
			  		if (scaler_restart()) {
			  			jpeg_encode_stop();
						scaler_stop();
						return 0;
				  	}

					if (!(jpeg_input_fifo_cnt & 0x1)) {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					} else {
						if (jpeg_encode_on_the_fly_start(scaler_output_fifo+fifo_size, NULL, NULL, fifo_size<<1)) {
							jpeg_encode_stop();
							scaler_stop();
							return 0;
						}
					}
					jpeg_input_fifo_cnt++;
					break;
			  	}
			}
		}
	}

	jpeg_encode_stop();
	scaler_stop();

	return fifo_encode_size;
}


/****************************************************************************/
/*
 *	jpeg_decode_for_capture
 */
void jpeg_decode_for_capture(INT32U srcW,INT32U srcH, INT32U srcInAddr, INT32U OutAddr)
{
	INT32S status;

	jpeg_init();
	jpeg_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);
	jpeg_image_size_set(srcW, srcH);
	jpeg_vlc_addr_set(srcInAddr);			// Input VLC(entropy encoded data) address
	jpeg_yuv_addr_set(OutAddr, 0, 0);		// Output addresses(16-byte alignment)
	jpeg_using_scaler_mode_enable();
	jpeg_decompression_start();

	while(1)
	{
		status = jpeg_status_polling(TRUE);
		if(status & C_JPG_STATUS_DECODE_DONE)
		{
			break;
		}
	}
}
 

INT32S scale_up_and_encode_for_capture(capture_photo_args_t *p_capture_photo_args)
{
	INT32S nRet;
	INT32U scale_input_buf;		//size1 = capture_photo_args.jpeg_img_width * capture_photo_args.jpeg_img_height * 2
	INT32U scale_output_buf;	//size2 = PIC_WIDTH * (32*4) * 2
	INT32U encode_buf;			// = scale_input_buf + size1 + size2
	INT32S i, num_loop;
	INT8U *ptr;
	INT32U total_size;
	INT32U restart_index = 0;
	INT32U input_width, input_height, pic_height;

	scale_input_buf = p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_max - 1].jpeg_buf_addrs + 
					  p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_max - 1].jpeg_vlc_size;
	scale_input_buf += 0x001f;
	scale_input_buf &= ~0x001f;
	
	scale_output_buf = scale_input_buf + p_capture_photo_args->jpeg_img_width * p_capture_photo_args->jpeg_img_height * 2; // + 1280*16*2
	scale_output_buf += 0x001f;
	scale_output_buf &= ~0x001f;
	
	encode_buf = scale_output_buf + PIC_WIDTH * 32 * 2;  // + 2560*32*2

	num_loop = (PIC_HEIGH / 32);
	//if((PIC_HEIGH % 32) != 0)
	//{
	//	num_loop += 1;
	//}

	restart_index = 0;
	total_size = 0;

	input_width = pAviEncVidPara->sensor_capture_width;
	pic_height = PIC_HEIGH;

	for(i=0; i<num_loop; i++)
	{
		jpeg_decode_for_capture(p_capture_photo_args->jpeg_img_width,
								p_capture_photo_args->jpeg_img_height,
								p_capture_photo_args->jpeg_compress_buf[i].jpeg_buf_addrs,
								scale_input_buf);

		//if(pic_height < 128)
		//{
		//	input_height = 8;
		//	nRet = scale_up_and_encode_fifo(scale_input_buf, input_width, input_height, scale_output_buf, PIC_WIDTH, pic_height, encode_buf, restart_index);
		//}
		//else
		//{
			input_height = 16;
			nRet = scale_up_and_encode_fifo(scale_input_buf, input_width, input_height, scale_output_buf, PIC_WIDTH, 32, encode_buf, restart_index);
			pic_height -= 32;
		//}

		if(nRet < 0)
		{
			DBG_PRINT("scale_up_and_encode_for_capture fail!\r\n");
			RETURN(STATUS_FAIL);
		}

		if(i==(num_loop - 1))
		{
			ptr = (INT8U *)(encode_buf + nRet);
			ptr -= 1;
			*ptr = 0xd9;

			write(pAviEncPara->AviPackerCur->file_handle, encode_buf, nRet);
		}
		else
		{
			write(pAviEncPara->AviPackerCur->file_handle, encode_buf, nRet);
			restart_index++;
			if (restart_index >= 8) {
				restart_index = 0;
			}
		}
	}

	nRet = STATUS_OK;
Return:
	return nRet;
}
#endif

// jpeg once encode
INT32U jpeg_encode_once(INT32U quality_value, INT32U input_format, INT32U width, INT32U height, INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v, INT32U output_buffer)
{
	INT32S	jpeg_status;
	INT32U 	encode_size;
	
	scaler_lock();
	
	jpeg_encode_init();
	gplib_jpeg_default_huffman_table_load();			// Load default huffman table 

	jpeg_encode_input_size_set(width, height);
	jpeg_encode_input_format_set(input_format);			// C_JPEG_FORMAT_YUYV / C_JPEG_FORMAT_YUV_SEPARATE
	jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);
	jpeg_encode_output_addr_set((INT32U) output_buffer);	
	jpeg_encode_once_start(input_buffer_y, input_buffer_u, input_buffer_v);
	
	while(1)
	{
		jpeg_status = jpeg_encode_status_query(TRUE);
		if(jpeg_status & C_JPG_STATUS_ENCODE_DONE) 
		{
			encode_size = jpeg_encode_vlc_cnt_get();		// Get encode length
			cache_invalid_range(output_buffer, encode_size);
			break;
		} 
		else if(jpeg_status & C_JPG_STATUS_ENCODING)
			continue;
		else
			DEBUG_MSG(DBG_PRINT("JPEG encode error!\r\n"));
	}
	jpeg_encode_stop();
	
	scaler_unlock();
	
	return encode_size;
}

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
INT32S jpeg_encode_fifo_start_for_capture(capture_photo_args_t *p_capture_photo_args)
{
	INT32S nRet;
	INT32U jpeg_output_addrs, next_jpeg_output_addrs;
	INT32U jpeg_status, encode_size;

	// JPEG input fifo mode
	jpeg_encode_init();
	gplib_jpeg_default_huffman_table_load();

	nRet = jpeg_encode_input_format_set(C_JPEG_FORMAT_YUYV);
	if(nRet < 0) RETURN(STATUS_FAIL);
	nRet = jpeg_encode_input_size_set(p_capture_photo_args->jpeg_img_width, p_capture_photo_args->jpeg_img_height); // Width and height(A/B fifo) of the image that will be compressed
	if(nRet < 0) RETURN(STATUS_FAIL);
	
	nRet = jpeg_encode_yuv_sampling_mode_set(p_capture_photo_args->jpeg_img_format); // encode_mode: C_JPG_CTRL_YUV422 or C_JPG_CTRL_YUV420(only valid for C_JPEG_FORMAT_YUV_SEPARATE format)
	if(nRet < 0) RETURN(STATUS_FAIL);
	jpeg_output_addrs = p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_idx].jpeg_buf_addrs;
	nRet = jpeg_encode_output_addr_set(jpeg_output_addrs);  // The address that VLC(variable length coded) data will be output
	if(nRet < 0) RETURN(STATUS_FAIL);

	if (jpeg_encode_on_the_fly_start(p_capture_photo_args->jpeg_input_addrs, NULL, NULL, p_capture_photo_args->jpeg_input_size<<2)) {
		jpeg_encode_stop();
		RETURN(STATUS_FAIL);
	}

	while (1) {
  		jpeg_status = jpeg_encode_status_query(1);
  		if (jpeg_status & C_JPG_STATUS_ENCODE_DONE) {
  			encode_size = jpeg_encode_vlc_cnt_get();
  			cache_invalid_range(jpeg_output_addrs+624, encode_size);

			p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_idx].jpeg_vlc_size = encode_size;
			next_jpeg_output_addrs = p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_idx].jpeg_buf_addrs + encode_size;
			next_jpeg_output_addrs += 0x1f;
			next_jpeg_output_addrs &= ~0x1f;

			p_capture_photo_args->jpeg_compress_buf_idx++;
			if(p_capture_photo_args->jpeg_compress_buf_idx >= p_capture_photo_args->jpeg_compress_buf_max)
			{
				jpeg_encode_stop();
				break;
			}
			p_capture_photo_args->jpeg_compress_buf[p_capture_photo_args->jpeg_compress_buf_idx].jpeg_buf_addrs = next_jpeg_output_addrs;
			break;
		}
		else
		{
			RETURN(STATUS_FAIL);
		}
	}
	nRet = STATUS_OK;
Return:
	return nRet;
}
#endif

extern void dynamic_tune_jpeg_Q(INT32U vlcSize);

// jpeg fifo encode
INT32S jpeg_encode_fifo_start(INT32U wait_done, INT32U quality_value, INT32U input_format, INT32U width, INT32U height, 
							INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v, 
							INT32U output_buffer, INT32U input_y_len, INT32U input_uv_len)
{
	INT32S nRet;
	
	jpeg_encode_init();
	gplib_jpeg_default_huffman_table_load();			        // Load default huffman table 
	nRet = jpeg_encode_input_size_set(width, height);
	if(nRet < 0) RETURN(STATUS_FAIL);
	
	nRet = jpeg_encode_input_format_set(input_format);
	if(nRet < 0) RETURN(STATUS_FAIL);
		
  	//if (pAviEncVidPara->encode_width == 1280) {
  	//	nRet = jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV420);
  	//} else {
		nRet = jpeg_encode_yuv_sampling_mode_set(C_JPG_CTRL_YUV422);
  	//}
	if(nRet < 0) RETURN(STATUS_FAIL);
	
	nRet = jpeg_encode_output_addr_set((INT32U)output_buffer);
	if(nRet < 0) RETURN(STATUS_FAIL);

	jpeg_vlc_maximum_length_set(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);
	nRet = jpeg_encode_on_the_fly_start(input_buffer_y, input_buffer_u, input_buffer_v, (input_y_len+input_uv_len+input_uv_len) << 1);
	if(nRet < 0) RETURN(STATUS_FAIL);
	
	if(wait_done == 0) RETURN(STATUS_OK);
	nRet = jpeg_encode_status_query(TRUE);	//wait jpeg block encode done 
Return:
	return nRet;
}

INT32S jpeg_encode_fifo_once(INT32U wait_done, INT32U jpeg_status, INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v,
							INT32U input_y_len, INT32U input_uv_len)
{
	INT32S nRet;
	
	while(1)
	{
		if(wait_done == 0) 
			jpeg_status = jpeg_encode_status_query(TRUE);

  		if (jpeg_status & C_JPG_STATUS_OUTPUT_FULL) {

			dynamic_tune_jpeg_Q(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);

  		    DBG_PRINT("C_JPG_STATUS_OUTPUT_FULL_1\r\n");
  		    RETURN(STATUS_FAIL);
  		}

	   	if(jpeg_status & C_JPG_STATUS_ENCODE_DONE) 
	    {
	    	RETURN(C_JPG_STATUS_ENCODE_DONE);
	    }
	    else if(jpeg_status & C_JPG_STATUS_INPUT_EMPTY)
	    {
			jpeg_vlc_maximum_length_set(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);
	    	nRet = jpeg_encode_on_the_fly_start(input_buffer_y, input_buffer_u, input_buffer_v, (input_y_len+input_uv_len+input_uv_len) << 1);
	    	if(nRet < 0) RETURN(nRet);
	    	
	    	if(wait_done == 0) RETURN(STATUS_OK);
	    	jpeg_status = jpeg_encode_status_query(TRUE);	//wait jpeg block encode done 
	  		if (jpeg_status & C_JPG_STATUS_OUTPUT_FULL) {

				dynamic_tune_jpeg_Q(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);

	  		    DBG_PRINT("C_JPG_STATUS_OUTPUT_FULL_2\r\n");
	  		    RETURN(STATUS_FAIL);
	  		}

	    	RETURN(jpeg_status);
	    }
	    else if(jpeg_status & C_JPG_STATUS_STOP)
	    {
	        DEBUG_MSG(DBG_PRINT("\r\njpeg encode is not started!\r\n"));
	    	RETURN(C_JPG_STATUS_STOP);
	    }
	    else if(jpeg_status & C_JPG_STATUS_TIMEOUT)
	    {
	        DEBUG_MSG(DBG_PRINT("\r\njpeg encode execution timeout1!\r\n"));
	        RETURN(C_JPG_STATUS_TIMEOUT);
	    }
	    else if(jpeg_status & C_JPG_STATUS_INIT_ERR)
	    {
	         DEBUG_MSG(DBG_PRINT("\r\njpeg encode init error!\r\n"));
	         RETURN(C_JPG_STATUS_INIT_ERR);
	    }
	    else
	    {
	    	DEBUG_MSG(DBG_PRINT("\r\nJPEG status error = 0x%x!\r\n", jpeg_status));
	        RETURN(STATUS_FAIL);
	    }
    }
	
Return:
	return nRet;
}

INT32S jpeg_encode_fifo_stop(INT32U wait_done, INT32U jpeg_status, INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v,
							INT32U input_y_len, INT32U input_uv_len)
{
	INT32S nRet;
	
	if(wait_done == 0) 
		jpeg_status = jpeg_encode_status_query(TRUE);

	if (jpeg_status & C_JPG_STATUS_OUTPUT_FULL) {

		dynamic_tune_jpeg_Q(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);

	    DBG_PRINT("C_JPG_STATUS_OUTPUT_FULL_1\r\n");
	    RETURN(STATUS_FAIL);
	}

	while(1)
	{
		if(jpeg_status & C_JPG_STATUS_ENCODE_DONE)
		{
			nRet = jpeg_encode_vlc_cnt_get();	// get jpeg encode size
			RETURN(nRet);
		}
		else if(jpeg_status & C_JPG_STATUS_INPUT_EMPTY)
		{
			jpeg_vlc_maximum_length_set(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);
			nRet = jpeg_encode_on_the_fly_start(input_buffer_y, input_buffer_u, input_buffer_v, (input_y_len+input_uv_len+input_uv_len) << 1);
			if(nRet < 0) RETURN(STATUS_FAIL);
			jpeg_status = jpeg_encode_status_query(TRUE);	//wait jpeg block encode done 
	  		if (jpeg_status & C_JPG_STATUS_OUTPUT_FULL) {

				dynamic_tune_jpeg_Q(AVI_ENCODE_VIDEO_BUFFER_SIZE - 624);

	  		    DBG_PRINT("C_JPG_STATUS_OUTPUT_FULL_3\r\n");
	  		    RETURN(STATUS_FAIL);
	  		}
		}
		else if(jpeg_status & C_JPG_STATUS_STOP)
	    {
	        DEBUG_MSG(DBG_PRINT("\r\njpeg encode is not started!\r\n"));
	    	RETURN(STATUS_FAIL);
	    }
	    else if(jpeg_status & C_JPG_STATUS_TIMEOUT)
	    {
	        DEBUG_MSG(DBG_PRINT("\r\njpeg encode execution timeout2!\r\n"));
	        RETURN(STATUS_FAIL);
	    }
	    else if(jpeg_status & C_JPG_STATUS_INIT_ERR)
	    {
	         DEBUG_MSG(DBG_PRINT("\r\njpeg encode init error!\r\n"));
	         RETURN(STATUS_FAIL);
	    }
	    else
	    {
	    	DEBUG_MSG(DBG_PRINT("\r\nJPEG status error stop = 0x%x!\r\n", jpeg_status));
	        RETURN(STATUS_FAIL);
	    }
	}
	
Return:
	jpeg_encode_stop();
	return nRet;
}

INT32S save_jpeg_file(INT16S fd, INT32U encode_frame, INT32U encode_size)
{
	INT32S nRet;
	
	nRet = write(fd, encode_frame, encode_size);
	if(nRet != encode_size) RETURN(STATUS_FAIL);	
	nRet = close(fd);
	if(nRet < 0) RETURN(STATUS_FAIL);
	fd = -1;
	nRet = STATUS_OK;	
Return:
	return nRet;
}

//audio
INT32S avi_audio_memory_allocate(INT32U	cblen)
{
	INT32U ptr;
	INT32S i, nRet;

	cblen = (cblen + 511) & ~511;
	ptr = (INT32U) gp_malloc_align(cblen * AVI_ENCODE_PCM_BUFFER_NO, 16);
	if (!ptr) {
		for (i=0; i<AVI_ENCODE_PCM_BUFFER_NO; i++) {
			pAviEncAudPara->pcm_input_addr[i] = NULL;
		}
		return STATUS_FAIL;
	}

	for(i=0; i<AVI_ENCODE_PCM_BUFFER_NO; i++) {
		pAviEncAudPara->pcm_input_addr[i] = ptr;
		ptr += cblen;
	}

	nRet = STATUS_OK;
	return nRet;
}

void avi_audio_memory_free(void)
{
	INT32U i;
	
	if (pAviEncAudPara->pcm_input_addr[0]) {
		gp_free((void *) pAviEncAudPara->pcm_input_addr[0]);
	}

	for(i=0; i<AVI_ENCODE_PCM_BUFFER_NO; i++)
	{
		pAviEncAudPara->pcm_input_addr[i] = 0;
	}	
}

INT32U avi_audio_get_next_buffer(void)
{
	INT32U addr;
	
	addr = pAviEncAudPara->pcm_input_addr[g_pcm_index++];
	if(g_pcm_index >= AVI_ENCODE_PCM_BUFFER_NO)
		g_pcm_index = 0;
	
	return addr;
}

INT32S avi_adc_double_buffer_put(INT16U *data,INT32U cwlen, OS_EVENT *os_q)
{
	INT32S status;

	g_avi_adc_dma_dbf.s_addr = (INT32U) P_MIC_ASADC_DATA;
	g_avi_adc_dma_dbf.t_addr = (INT32U) data;
	g_avi_adc_dma_dbf.width = DMA_DATA_WIDTH_2BYTE;		
	g_avi_adc_dma_dbf.count = (INT32U) cwlen;
	g_avi_adc_dma_dbf.notify = NULL;
	g_avi_adc_dma_dbf.timeout = 0;
	status = dma_transfer_with_double_buf(&g_avi_adc_dma_dbf, os_q);
	if(status < 0) return status;
	return STATUS_OK;
}

INT32U avi_adc_double_buffer_set(INT16U *data, INT32U cwlen)
{
	INT32S status;

	g_avi_adc_dma_dbf.s_addr = (INT32U) P_MIC_ASADC_DATA;
	g_avi_adc_dma_dbf.t_addr = (INT32U) data;
	g_avi_adc_dma_dbf.width = DMA_DATA_WIDTH_2BYTE;		
	g_avi_adc_dma_dbf.count = (INT32U) cwlen;
	g_avi_adc_dma_dbf.notify = NULL;
	g_avi_adc_dma_dbf.timeout = 0;
	status = dma_transfer_double_buf_set(&g_avi_adc_dma_dbf);
	if(status < 0) return status;
	return STATUS_OK;
}

INT32S avi_adc_dma_status_get(void)
{
	if (g_avi_adc_dma_dbf.channel == 0xff) 
		return -1;
	
	return dma_status_get(g_avi_adc_dma_dbf.channel);	
}

void avi_adc_double_buffer_free(void)
{
	dma_transfer_double_buf_free(&g_avi_adc_dma_dbf);
	g_avi_adc_dma_dbf.channel = 0xff;
}

void avi_adc_hw_start(void)
{
//	R_MIC_SETUP = 0;
	R_MIC_SETUP = 0xC000;
	R_MIC_ASADC_CTRL = 0;
	R_MIC_SH_WAIT = 0x0807;
	
	mic_fifo_clear();	
	mic_fifo_level_set(4);
	//R_MIC_SETUP = 0xA0C0; //enable AGC
//	R_MIC_SETUP = 0xb080;
	R_MIC_SETUP = 0xF080;
	mic_auto_sample_start();
}

void avi_adc_hw_stop(void)
{
	mic_auto_sample_stop();
}
