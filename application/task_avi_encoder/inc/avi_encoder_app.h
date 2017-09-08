#ifndef __AVI_ENCODER_APP_H__
#define __AVI_ENCODER_APP_H__

#include "application.h"
#include "avi_encoder_scaler_jpeg.h"

typedef struct AviEncAudPara_s
{
	//audio encode
	INT32U  audio_format;			// audio encode format
	INT16U  audio_sample_rate;		// sample rate
	INT16U  channel_no;				// channel no, 1:mono, 2:stereo
	//INT8U   *work_mem;				// wave encode work memory 
	//INT32U  pack_size;				// wave encode package size in byte
	//INT32U  pack_buffer_addr;
	//INT32U  pcm_input_size;			// wave encode pcm input size in short
	INT32U  pcm_input_addr[AVI_ENCODE_PCM_BUFFER_NO];
}AviEncAudPara_t;

typedef struct AviEncVidPara_s
{
	//sensor input 
    INT32U  sensor_output_format;   // sensor output format
    INT16U  sensor_capture_width;   // sensor width
    INT16U  sensor_capture_height;  // sensor height
    INT32U  csi_frame_addr[AVI_ENCODE_CSI_BUFFER_NO];	// sensor input buffer addr
    
    //scaler output
    INT16U 	scaler_flag;
    INT16U 	dispaly_scaler_flag;  	// 0: not scaler, 1: scaler again for display 
    FP32    scaler_zoom_ratio;      // for zoom scaler   
    INT32U  scaler_output_addr[AVI_ENCODE_SCALER_BUFFER_NO];	// scaler output buffer addr
      								
    //display scaler
    INT32U  display_output_format;  // for display use
    INT16U  display_width;          // display width
    INT16U  display_height;         // display height
    INT16U  display_buffer_width;   // display width
    INT16U  display_buffer_height;  // display height 
    INT32U  display_output_addr[AVI_ENCODE_DISPALY_BUFFER_NO];	// display scaler buffer addr
    
    //video encode
    INT32U  video_format;			// video encode format
    INT8U   dwScale;				// dwScale
    INT8U   dwRate;					// dwRate 
    INT16U  quality_value;			// video encode quality
    INT16U  encode_width;           // video encode width
    INT16U  encode_height;          // videoe ncode height
    AVIPACKER_FRAME_INFO  video_encode_addr[AVI_ENCODE_VIDEO_BUFFER_NO]; //video encode buffer
	
	//mpeg4 encode use
	INT32U  isram_addr;
	INT32U  write_refer_addr;
	INT32U  reference_addr;
}AviEncVidPara_t;

typedef struct AviEncPacker_s
{
	void 	*avi_workmem;
	INT16S  file_handle;
    
    //avi video info
    GP_AVI_AVISTREAMHEADER *p_avi_vid_stream_header;
    INT32U  bitmap_info_cblen;		// bitmap header length
    GP_AVI_BITMAPINFO *p_avi_bitmap_info;
    
    //avi audio info
    GP_AVI_AVISTREAMHEADER *p_avi_aud_stream_header;
    INT32U  wave_info_cblen;		// wave header length
    GP_AVI_PCMWAVEFORMAT *p_avi_wave_info;
    
    //avi packer 
	INT32U  task_prio;
	void    *index_write_buffer;
	INT32U	index_buffer_size;		// AviPacker index buffer size in byte

	INT8U print_flag;
}AviEncPacker_t;

typedef struct AviEncPara_s
{   
	INT8U  source_type;				// SOURCE_TYPE_FS or SOURCE_TYPE_USER_DEFINE
    INT8U  key_flag;
    INT8U  fifo_enc_err_flag;
    INT8U  fifo_irq_cnt;
   	
    //avi file info
    INT32U  avi_encode_status;      // 0:IDLE
    AviEncPacker_t *AviPackerCur;
    
	//allow record size
	INT64U  disk_free_size;			// disk free size
	INT32U  record_total_size;		// AviPacker storage to disk total size
    
	//aud & vid sync
	INT64S  delta_ta, tick;
    INT64S  delta_Tv;
    INT32S  freq_div;
    INT64S  ta, tv, Tv;
   	INT32U  pend_cnt, post_cnt;
	
	INT32U  ready_frame;
	INT32S  ready_size;
	INT32U  vid_pend_cnt, vid_post_cnt;
}AviEncPara_t;

typedef struct AviEncVidReadyBuf
{
	INT32U	ready_frame;
	INT32S	ready_size;
	INT8U	key_flag;
}AviEncVidReadyBuf_t;

//extern os-event
extern OS_EVENT *AVIEncodeApQ;
extern OS_EVENT *scaler_task_q;
extern OS_EVENT *cmos_frame_q;
extern OS_EVENT *vid_enc_task_q;
extern OS_EVENT *avi_aud_q;
extern OS_EVENT *scaler_hw_sem;

extern AviEncPara_t *pAviEncPara;
extern AviEncAudPara_t *pAviEncAudPara;
extern AviEncVidPara_t *pAviEncVidPara;
extern AviEncPacker_t *pAviEncPacker0, *pAviEncPacker1;

extern INT32S time_stamp_buffer_size;
extern INT32U time_stamp_buffer[AVI_ENCODE_VIDEO_BUFFER_NO];

extern INT8U JpegSendFlag;
extern INT32U first_time;
extern INT8U g_video_index;
extern INT8U jpeg_avi_header[624];
extern INT32S curr_Q_value;

#if C_UVC == CUSTOM_ON
//USB Webcam 
extern OS_EVENT *usbwebcam_frame_q;
extern OS_EVENT  *USBCamApQ;
extern OS_EVENT *USBAudioApQ;
typedef struct   
{
    INT32U FrameSize;
    INT32U  AddrFrame;
    INT8U IsoSendState;
}ISOTaskMsg;
#endif

//jpeg header
extern INT8U jpeg_422_q90_header[];

//mpeg4 encoder
#if MPEG4_ENCODE_ENABLE == 1
	extern void mpeg4_encode_init(void);
	extern INT32S mpeg4_encode_config(INT8U input_format, INT16U width, INT16U height, INT8U vop_time_inc_reso);
	extern void mpeg4_encode_ip_set(INT8U byReg, INT32U IPXXX_No);
	extern INT32S mpeg4_encode_isram_set(INT32U addr);
	extern INT32S mpeg4_encode_start(INT8U ip_frame, INT8U quant_value, INT32U vlc_out_addr, 
									INT32U raw_data_in_addr, INT32U encode_out_addr, INT32U refer_addr);
	extern INT32S mpeg4_encode_get_vlc_size(void);
	extern INT32S mpeg4_wait_idle(INT8U wait_done);
	extern void mpeg4_encode_stop(void);
#endif

//avi encode state
extern INT32U avi_enc_packer_init(AviEncPacker_t *pAviEncPacker);
extern INT32U avi_enc_packer_start(AviEncPacker_t *pAviEncPacker);
extern INT32U avi_enc_packer_stop(AviEncPacker_t *pAviEncPacker); 
extern INT32S vid_enc_preview_start(void);
extern INT32S vid_enc_preview_stop(void);
extern INT32S avi_enc_start(void);
extern INT32S avi_enc_stop(void);
extern INT32S avi_enc_pause(void);
extern INT32S avi_enc_resume(void);
extern INT32S avi_enc_save_jpeg(void);
extern INT32S avi_enc_storage_full(void);
extern INT32S avi_packer_err_handle(INT32S ErrCode);
extern INT32S avi_encode_state_task_create(INT8U pori);
extern INT32S avi_encode_state_task_del(void);
extern void   avi_encode_state_task_entry(void *para);
extern void  (*pfn_avi_encode_put_data)(void *WorkMem, AVIPACKER_FRAME_INFO* pFrameInfo);

//scaler task
extern INT32S scaler_task_create(INT8U pori);
extern INT32S scaler_task_del(void);
extern INT32S scaler_task_start(void);
extern INT32S scaler_task_stop(void);
extern void   scaler_task_entry(void *parm);

//video_encode task
extern INT32S video_encode_task_create(INT8U pori);
extern INT32S video_encode_task_del(void);
extern INT32S video_encode_task_start(void);
extern INT32S video_encode_task_stop(void);
extern void   video_encode_task_entry(void *parm);

//audio task
extern INT32S avi_adc_record_task_create(INT8U pori);
extern INT32S avi_adc_record_task_del(void);
extern INT32S avi_audio_record_start(void);
extern INT32S avi_audio_record_restart(void);
extern INT32S avi_audio_record_stop(void);
extern void avi_audio_record_entry(void *parm);

//avi encode api
extern void   avi_encode_init(void);

//extern void avi_encode_video_timer_start(void);
extern void avi_encode_video_timer_stop(void);
extern void avi_encode_audio_timer_start(void);
extern void avi_encode_audio_timer_stop(void);

extern INT32S avi_encode_set_file_handle_and_caculate_free_size(AviEncPacker_t *pAviEncPacker, INT16S FileHandle);
extern INT16S avi_encode_close_file(AviEncPacker_t *pAviEncPacker);
extern INT32S avi_encode_set_avi_header(AviEncPacker_t *pAviEncPacker);
extern void avi_encode_set_curworkmem(void *pAviEncPacker);
//status
extern void   avi_encode_set_status(INT32U bit);
extern void   avi_encode_clear_status(INT32U bit);
extern INT32S avi_encode_get_status(void);
//memory
extern INT32U avi_encode_get_csi_frame(void);
extern INT32U avi_encode_get_scaler_frame(void);
extern INT32U avi_encode_get_display_frame(void);
extern INT32U avi_encode_get_video_frame(void);
extern INT32S avi_encode_memory_alloc(void);
extern void avi_encode_memory_free(void);
extern INT32S avi_encode_mpeg4_malloc(INT16U width, INT16U height);
extern void avi_encode_mpeg4_free(void);

//format
extern void avi_encode_set_sensor_format(INT32U format);
extern void avi_encode_set_display_format(INT32U format);
//other
extern void avi_encode_set_display_scaler(void);
extern INT32S avi_encode_set_jpeg_quality(INT32U addr, INT8U quality_value);
extern INT32S avi_encode_set_mp4_resolution(INT16U encode_width, INT16U encode_height);
extern BOOLEAN avi_encode_is_pause(void);
extern INT32S avi_encode_disk_size_is_enough(INT32S cb_write_size);

extern void avi_encode_switch_csi_frame_buffer(void);
extern void vid_enc_csi_fifo_end(INT8U flag, INT8U type);

extern INT32S scaler_zoom_once(INT32U scaler_mode, FP32 bScalerFactor, 
							INT32U InputFormat, INT32U OutputFormat,
							INT16U input_x, INT16U input_y, 
							INT16U output_x, INT16U output_y,
							INT16U output_buffer_x, INT16U output_buffer_y, 
							INT32U scaler_input_y, INT32U scaler_input_u, INT32U scaler_input_v, 
							INT32U scaler_output_y, INT32U scaler_output_u, INT32U scaler_output_v);
extern INT32S scale_up_and_encode(INT32U sensor_input_addr, INT32U scaler_output_fifo, INT32U jpeg_encode_width, INT32U jpeg_encode_height, INT32U jpeg_output_addr);						
extern INT32U jpeg_encode_once(INT32U quality_value, INT32U input_format, INT32U width, INT32U height, 
							INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v, INT32U output_buffer);
// jpeg fifo encode
extern INT32S jpeg_encode_fifo_start(INT32U wait_done, INT32U quality_value, INT32U input_format, INT32U width, INT32U height, 
							INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v, 
							INT32U output_buffer, INT32U input_y_len, INT32U input_uv_len);
extern INT32S jpeg_encode_fifo_once(INT32U wait_done, INT32U jpeg_status, INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v,
							INT32U input_y_len, INT32U input_uv_len);		
extern INT32S jpeg_encode_fifo_stop(INT32U wait_done, INT32U jpeg_status, INT32U input_buffer_y, INT32U input_buffer_u, INT32U input_buffer_v,
							INT32U input_y_len, INT32U input_uv_len);

#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
extern INT32S jpeg_encode_fifo_start_for_capture(capture_photo_args_t *p_capture_photo_args);  //add for capture photo compressing with interval
extern INT32S scale_up_and_encode_for_capture(capture_photo_args_t *p_capture_photo_args);
#endif

extern INT32S save_jpeg_file(INT16S fd, INT32U encode_frame, INT32U encode_size);
extern INT32S avi_audio_memory_allocate(INT32U	cblen);
extern void avi_audio_memory_free(void);
extern INT32U avi_audio_get_next_buffer(void);

extern INT32S avi_adc_double_buffer_put(INT16U *data,INT32U cwlen, OS_EVENT *os_q);
extern INT32U avi_adc_double_buffer_set(INT16U *data, INT32U cwlen);
extern INT32S avi_adc_dma_status_get(void);
extern void avi_adc_double_buffer_free(void);
extern void avi_adc_hw_start(void);
extern void avi_adc_hw_stop(void);

extern void mic_fifo_clear(void);
extern void mic_fifo_level_set(INT8U level);
extern INT32S mic_auto_sample_start(void);
extern void mic_auto_sample_stop(void);
extern INT32S mic_sample_rate_set(INT8U timer_id, INT32U hz);
extern INT32S mic_timer_stop(INT8U timer_id);

extern INT8U capture_flag;

#endif // __AVI_ENCODER_APP_H__
