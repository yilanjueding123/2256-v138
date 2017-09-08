#include "state_video_preview.h"

extern void ap_video_preview_init(void);
extern void ap_video_preview_exit(void);
extern void ap_video_preview_func_key_active(void);
extern void ap_video_preview_sts_set(INT8S sts);
extern INT8U ap_video_preview_sts_get(void);
extern INT32S ap_video_preview_reply_action(STOR_SERV_FILEINFO *file_info_ptr);
extern void ap_video_preview_reply_done(INT8U ret, INT32U file_path_addr);
