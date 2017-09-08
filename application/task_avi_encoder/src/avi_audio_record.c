#include "avi_audio_record.h"
#include "lpf.h"

#define C_AVI_AUDIO_RECORD_STACK_SIZE	512
#define C_AVI_AUD_ACCEPT_MAX			5

#define USB_AUDIO_PCM_SAMPLES (2*1024)
#define AVI_AUDIO_PCM_SAMPLES (AVI_AUD_SAMPLING_RATE)

/*os task stack */
INT32U	AviAudioRecordStack[C_AVI_AUDIO_RECORD_STACK_SIZE];

/* os task queue */
OS_EVENT *avi_aud_q;
OS_EVENT *avi_aud_ack_m;
void *avi_aud_q_stack[C_AVI_AUD_ACCEPT_MAX];

AVIPACKER_FRAME_INFO audio_frame[AVI_ENCODE_PCM_BUFFER_NO] = {0};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
INT32S avi_adc_record_task_create(INT8U pori)
{	
	INT8U  err;
	INT32S nRet;
	
	avi_aud_q = OSQCreate(avi_aud_q_stack, C_AVI_AUD_ACCEPT_MAX);
	if(!avi_aud_q) RETURN(STATUS_FAIL);
	
	avi_aud_ack_m = OSMboxCreate(NULL);
	if(!avi_aud_ack_m) RETURN(STATUS_FAIL);
	
	err = OSTaskCreate(avi_audio_record_entry, NULL, (void *)&AviAudioRecordStack[C_AVI_AUDIO_RECORD_STACK_SIZE - 1], pori);
	if(err != OS_NO_ERR) RETURN(STATUS_FAIL);
	
	nRet = STATUS_OK;
Return:
    return nRet;
}

INT32S avi_adc_record_task_del(void)
{
	INT8U  err;
	INT32U nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(avi_aud_q, AVI_AUDIO_RECORD_EXIT, avi_aud_ack_m, 5000, msg, err);
Return:	
	OSQFlush(avi_aud_q);
   	OSQDel(avi_aud_q, OS_DEL_ALWAYS, &err);
	OSMboxDel(avi_aud_ack_m, OS_DEL_ALWAYS, &err);
	return nRet;
}

INT32S avi_audio_record_start(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(avi_aud_q, AVI_AUDIO_RECORD_START, avi_aud_ack_m, 5000, msg, err);
Return:		
	return nRet;	
}

INT32S avi_audio_record_restart(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(avi_aud_q, AVI_AUDIO_RECORD_RESTART, avi_aud_ack_m, 5000, msg, err);
Return:
	return nRet;
}

INT32S avi_audio_record_stop(void)
{
	INT8U  err;
	INT32S nRet, msg;
	
	nRet = STATUS_OK;
	POST_MESSAGE(avi_aud_q, AVI_AUDIO_RECORD_STOPING, avi_aud_ack_m, 5000, msg, err);
Return:	
	if(nRet < 0)
	{
		avi_audio_memory_free();
	}
	return nRet;	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void avi_audio_record_entry(void *parm)
{
	INT8U   err, bStop, audio_flag, write_audio_idx;
	INT32S  nRet, audio_stream, i;
	INT32U  msg_id, pcm_addr, pcm_cwlen;
	INT32U  ready_addr, next_addr;

#if C_USB_AUDIO == CUSTOM_ON
	ISOTaskMsg audioios;
#endif

#if C_LPF_ENABLE ==1
	INT32U coef_freq, i;
	INT16U coef_loop, t, *ptr;
#endif	 

	while(1) 
	{
		msg_id = (INT32U) OSQPend(avi_aud_q, 0, &err);
		if(err != OS_NO_ERR)
			continue;

		switch(msg_id)
		{
			case AVI_AUDIO_DMA_DONE:
				if(bStop)
				{
					avi_adc_double_buffer_set((INT16U*) 0x40000000, pcm_cwlen);
					OSQPost(avi_aud_q, (void *)AVI_AUDIO_RECORD_STOP);  // check dma is done and stop
					break;
				}

			#if C_USB_AUDIO == CUSTOM_ON
				if((avi_encode_get_status()&C_AVI_ENCODE_USB_WEBCAM))
				{
					pcm_addr = ready_addr;
					ready_addr = next_addr;
					next_addr = avi_audio_get_next_buffer();
					avi_adc_double_buffer_set((INT16U*)next_addr, pcm_cwlen);// set dma buffer

					audioios.FrameSize = USB_AUDIO_PCM_SAMPLES*2;
					audioios.AddrFrame = pcm_addr;
					OSQPost(USBAudioApQ, (void*)(&audioios));
					break;
				} else
			#endif
				{
					if(ready_addr == (audio_frame[0].buffer_addrs+8)) {
	                    write_audio_idx = 0;
					} else if(ready_addr == (audio_frame[1].buffer_addrs+8)) {
	                    write_audio_idx = 1;
					} else if(ready_addr == (audio_frame[2].buffer_addrs+8)) {
	                    write_audio_idx = 2;
					} else {
						DBG_PRINT("audio buffer error!\r\n");
						while(1);
					}
				}

				for(i=0; i<AVI_ENCODE_PCM_BUFFER_NO; i++) {
					if((audio_frame[i].buffer_addrs + 8) == next_addr) {
						continue;
					}

					if(audio_frame[i].is_used == 0) {
						break;
					}
				}

				if(i == AVI_ENCODE_PCM_BUFFER_NO)
				{	// waiting avi packer, if there is no audio buffer
					DBG_PRINT("&");

					avi_adc_double_buffer_set((INT16U*) 0x40000000, pcm_cwlen);
					break;
				}

				ready_addr = next_addr;
				next_addr = audio_frame[i].buffer_addrs + 8;
				avi_adc_double_buffer_set((INT16U*)next_addr, pcm_cwlen);// set dma buffer

				pcm_addr = audio_frame[write_audio_idx].buffer_addrs;

#if (C_LPF_ENABLE ==1)
				cache_invalid_range((INT32U)pcm_addr, pcm_cwlen << 1);
				ptr = (INT16U *)pcm_addr;
				for(i=0; i<pcm_cwlen; i++)
				{
					t = *ptr;
					(INT16S)t = LPF_process((INT16S)t);	
					*ptr++=t;
				}
#endif
				audio_stream = (pcm_cwlen<<1) + 8;
				
				// when restart, wait pcm frame end
				if(audio_flag == 1) 
				{
					audio_flag = 0;
					OSMboxPost(avi_aud_ack_m, (void*)C_ACK_SUCCESS);
				}

				if((avi_encode_get_status()&C_AVI_ENCODE_START) == 0) break;
				
				nRet = avi_encode_disk_size_is_enough(audio_stream);
				if (nRet == 0) {
					avi_enc_storage_full();
					continue;
				} else if (nRet == 2) {
					msgQSend(ApQ, MSG_APQ_RECORD_SWITCH_FILE, NULL, NULL, MSG_PRI_NORMAL);
				}
				#if MUTE_SWITCH	
				gp_memset((INT8S*)(audio_frame[write_audio_idx].buffer_addrs+8), 0, (AVI_AUDIO_PCM_SAMPLES<<1)); 
				#endif
				
				if(pfn_avi_encode_put_data) {
					audio_frame[write_audio_idx].ext = (AVI_AUDIO_PCM_SAMPLES<<1)/pAviEncPara->AviPackerCur->p_avi_wave_info->nBlockAlign;
		        	audio_frame[write_audio_idx].buffer_len = ((((INT32U)(AVI_AUDIO_PCM_SAMPLES<<1) + 8 + 511) >> 9) << 9);
		        	audio_frame[write_audio_idx].is_used = 1;
		        	audio_frame[write_audio_idx].msg_id = AVIPACKER_MSG_AUDIO_WRITE;

					*(INT32S*)(audio_frame[write_audio_idx].buffer_addrs + (AVI_AUDIO_PCM_SAMPLES<<1) + 8) = (('J'&0xFF) | (('U'&0xFF)<<8) | (('N'&0xFF)<<16) | (('K'&0xFF)<<24));      // add "JUNK"
			        *(INT32S*)(audio_frame[write_audio_idx].buffer_addrs + (AVI_AUDIO_PCM_SAMPLES<<1) + 12) = audio_frame[write_audio_idx].buffer_len - (AVI_AUDIO_PCM_SAMPLES<<1) - 16; // add length

		        	pfn_avi_encode_put_data(pAviEncPara->AviPackerCur->avi_workmem, &audio_frame[write_audio_idx]);
				}
				break;

			case AVI_AUDIO_RECORD_START:
				bStop = audio_flag = 0;
			  #if C_USB_AUDIO == CUSTOM_ON
				if((avi_encode_get_status()&C_AVI_ENCODE_USB_WEBCAM))
				{
					pcm_cwlen = USB_AUDIO_PCM_SAMPLES;

					nRet = avi_audio_memory_allocate(pcm_cwlen<<1);
					if(nRet < 0)  {
						DBG_PRINT("Audio Buffer Alloc Fail\r\n");
						goto AUDIO_RECORD_START_FAIL;
					}
				  	DBG_PRINT("audlen=%d\r\n",pcm_cwlen);

					ready_addr = avi_audio_get_next_buffer();
					next_addr = avi_audio_get_next_buffer();

					nRet = avi_adc_double_buffer_put((INT16U*)(ready_addr), pcm_cwlen, avi_aud_q);
					if(nRet < 0) goto AUDIO_RECORD_START_FAIL;
					nRet = avi_adc_double_buffer_set((INT16U*)(next_addr), pcm_cwlen);
					if(nRet < 0) goto AUDIO_RECORD_START_FAIL;
				}
				else 
			  #endif
				{
					pcm_cwlen =  AVI_AUDIO_PCM_SAMPLES;  //////////joseph////////////////////////pAviEncAudPara->pcm_input_size * C_WAVE_ENCODE_TIMES;

					nRet = avi_audio_memory_allocate((pcm_cwlen<<1) + 8);
					if(nRet < 0) {
						DBG_PRINT("Audio Buffer Alloc Fail\r\n");
						goto AUDIO_RECORD_START_FAIL;
					}
				  	DBG_PRINT("audlen=%d\r\n",pcm_cwlen);

					for (i=0; i<AVI_ENCODE_PCM_BUFFER_NO; i++) {
					    audio_frame[i].is_used = 0;
					    audio_frame[i].buffer_addrs = avi_audio_get_next_buffer();
					    audio_frame[i].buffer_idx = i;
						*(INT32S*)(audio_frame[i].buffer_addrs) = (('0'&0xFF) | (('1'&0xFF)<<8) | (('w'&0xFF)<<16) | (('b'&0xFF)<<24));    // add "01wb"
				        *(INT32S*)(audio_frame[i].buffer_addrs+4) = (INT32U)(pcm_cwlen<<1);                 // add length
				    }

					ready_addr = audio_frame[0].buffer_addrs+8;
                    next_addr = audio_frame[1].buffer_addrs+8;
					
					nRet = avi_adc_double_buffer_put((INT16U*)(ready_addr), pcm_cwlen, avi_aud_q);
					if(nRet < 0) goto AUDIO_RECORD_START_FAIL;
					nRet = avi_adc_double_buffer_set((INT16U*)(next_addr), pcm_cwlen);
					if(nRet < 0) goto AUDIO_RECORD_START_FAIL;
				}
				avi_adc_hw_start();

#if C_USB_AUDIO == CUSTOM_ON
				if((avi_encode_get_status()&C_AVI_ENCODE_USB_WEBCAM))
				{
					OSMboxPost(avi_aud_ack_m, (void*)C_ACK_SUCCESS);
					break;
				}
#endif

#if (C_LPF_ENABLE ==1)
				coef_freq = Cut_Off_Freq;
				coef_loop = Filter_Order;			
				//LPF_init(pAudio_Encode_Para->SampleRate,3);
				LPF_init(coef_freq, coef_loop);
#endif
				//pAviEncPara->delta_ta = (INT64S)pAviEncVidPara->dwRate * pcm_cwlen;
				OSMboxPost(avi_aud_ack_m, (void*)C_ACK_SUCCESS);
				break;
AUDIO_RECORD_START_FAIL:
				avi_adc_hw_stop();
				avi_adc_double_buffer_free();
				avi_audio_memory_free();
				DBG_PRINT("AudEncStartFail!!!\r\n");
				OSMboxPost(avi_aud_ack_m, (void*)C_ACK_FAIL);
				break;	
				
			case AVI_AUDIO_RECORD_STOP:
				avi_adc_hw_stop();
				avi_adc_double_buffer_free();
				avi_audio_memory_free();
				OSMboxPost(avi_aud_ack_m, (void*)C_ACK_SUCCESS);
				break;
				
			case AVI_AUDIO_RECORD_STOPING:
				bStop = 1;
				break;
		
			case AVI_AUDIO_RECORD_RESTART:
				audio_flag = 1;
				break;
			
			case AVI_AUDIO_RECORD_EXIT:
				OSQFlush(avi_aud_q);
				OSMboxPost(avi_aud_ack_m, (void*)C_ACK_SUCCESS);
				OSTaskDel(OS_PRIO_SELF);
				break;
		}	
	}
}

/*

/////////////////////////////////////////////////////////////////////////////////////////////////////////
static INT32S avi_wave_encode_start(void)
{
	INT32S nRet, size;

	size = wav_enc_get_mem_block_size();
	pAviEncAudPara->work_mem = (INT8U *)gp_malloc(size);
	if(!pAviEncAudPara->work_mem) RETURN(STATUS_FAIL);
	gp_memset((INT8S*)pAviEncAudPara->work_mem, 0, size);
	nRet = wav_enc_Set_Parameter( pAviEncAudPara->work_mem, 
								  pAviEncAudPara->channel_no, 
								  pAviEncAudPara->audio_sample_rate, 
								  pAviEncAudPara->audio_format);
	if(nRet < 0) RETURN(STATUS_FAIL);
	nRet = wav_enc_init(pAviEncAudPara->work_mem);
	if(nRet < 0) RETURN(STATUS_FAIL);
	pAviEncAudPara->pcm_input_size = wav_enc_get_SamplePerFrame(pAviEncAudPara->work_mem);
	
	switch(pAviEncAudPara->audio_format)
	{
	case WAVE_FORMAT_PCM:
		pAviEncAudPara->pack_size = pAviEncAudPara->pcm_input_size;	
		pAviEncAudPara->pack_size *= 2;
		break;
	
	case WAVE_FORMAT_ALAW:
	case WAVE_FORMAT_MULAW:	
	case WAVE_FORMAT_ADPCM:
	case WAVE_FORMAT_IMA_ADPCM:
		pAviEncAudPara->pack_size = wav_enc_get_BytePerPackage(pAviEncAudPara->work_mem);	
		break;
	}
	
	nRet = STATUS_OK;
Return:	
	return nRet;	
}

static INT32S avi_wave_encode_stop(void)
{
	gp_free((void*)pAviEncAudPara->work_mem);
	pAviEncAudPara->work_mem = 0;
	return STATUS_OK;
}

static INT32S avi_wave_encode_once(INT16S *pcm_input_addr)
{
	INT8U *encode_output_addr;
	INT32S nRet, encode_size, N;
	
	encode_size = 0;
	N = C_WAVE_ENCODE_TIMES;
	encode_output_addr = (INT8U*)pAviEncAudPara->pack_buffer_addr;
	while(N--)
	{
		nRet = wav_enc_run(pAviEncAudPara->work_mem, (short *)pcm_input_addr, encode_output_addr);
		if(nRet < 0)		
			return  STATUS_FAIL;
		
		encode_size += nRet;
		pcm_input_addr += pAviEncAudPara->pcm_input_size;
		encode_output_addr += pAviEncAudPara->pack_size;
	}
	
	return encode_size;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

*/
