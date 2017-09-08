#include "task_storage_service.h"
#include "stdio.h"

#define STORAGE_TIME_INTERVAL_MOUNT		128	//128 = 1s
#define STORAGE_TIME_INTERVAL_FREESIZE	128*20	//128 = 1s
//#define ERROR_HANDLE_POWER_OFF			128*20	//128 = 1s

extern void ap_storage_service_init(void);
extern INT32S ap_storage_service_storage_mount(void);
#if C_AUTO_DEL_FILE == CUSTOM_ON
	extern void ap_storage_service_freesize_check_switch(INT8U type);
	extern INT32S ap_storage_service_freesize_check_and_del(void);
#endif
extern void ap_storage_service_file_open_handle(INT32U type);
extern void ap_storage_service_timer_start(void);
extern void ap_storage_service_timer_stop(void);
extern void JH_Message_Get(void);
#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
	extern void ap_storage_service_cyclic_record_file_open_handle(void);
#endif
extern void ap_storage_service_usb_plug_in(void);
//extern void ap_storage_service_format_req(void);