#include "task_state_handling.h"

#define VIDEO_PREVIEW_UNMOUNT				0x1
#define VIDEO_PREVIEW_BUSY					0x2

#define VIDEO_PREVIEW_MD_TIME_INTERVAL		64

extern void state_video_preview_entry(void *para);
extern void ap_video_preview_init(void);
extern void ap_video_preview_exit(void);
extern void ap_video_preview_func_key_active(void);
extern INT32S ap_video_preview_reply_action(STOR_SERV_FILEINFO *file_info_ptr);
extern void ap_video_preview_reply_done(INT8U ret, INT32U file_path_addr);
extern void ap_video_preview_sts_set(INT8S sts);
extern INT8U ap_video_preview_sts_get(void);