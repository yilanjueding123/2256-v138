#include "state_video_record.h"

#define VIDEO_RECORD_CYCLE_TIME_INTERVAL		128*60	//128 = 1s
#define MOTION_DETECT_CHECK_TIME_INTERVAL		128

extern INT8S ap_video_record_init(INT32U state);
extern void ap_video_record_exit(void);
#if C_MOTION_DETECTION == CUSTOM_ON	
	extern void ap_video_record_md_tick(INT8U *md_tick,INT32U state);
	extern INT8S ap_video_record_md_active(INT8U *md_tick,INT32U state);
	extern void ap_video_record_md_disable(void);
#endif	
extern INT8S ap_video_record_func_key_active(INT32U state);
extern void ap_video_record_sts_set(INT8S sts);
#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
	extern void ap_video_record_cycle_reply_action(STOR_SERV_FILEINFO *file_info_ptr);
#endif
extern void ap_video_record_error_handle(INT32U state);
extern INT8U ap_video_record_sts_get(void);
extern void ap_video_record_usb_handle(void);
extern void bkground_del_disable(INT32U disable1_enable0);
extern INT8U ap_audio_record_sts_get(void);
