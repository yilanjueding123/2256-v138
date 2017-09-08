#include "ap_video_record.h"

static STOR_SERV_FILEINFO curr_file_info;
INT8S video_record_sts;

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
	static INT8U cyclic_record_timerid;
	static STOR_SERV_FILEINFO next_file_info;
    static INT8S g_lock_current_file_flag = 0;
#endif

#if C_MOTION_DETECTION == CUSTOM_ON
	static INT8U motion_detect_timerid = 0xFF;
#endif

INT8S ap_video_record_init(INT32U state)
{
	//INT32U led_type;
	if (ap_state_handling_storage_id_get() == NO_STORAGE) {
		ap_video_record_sts_set(VIDEO_RECORD_UNMOUNT);
		return STATUS_FAIL;
	} else {
		ap_video_record_sts_set(~VIDEO_RECORD_UNMOUNT);

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
		cyclic_record_timerid = 0xFF;
		next_file_info.file_handle = -1;
#endif

		if (state == STATE_VIDEO_RECORD){
			//led_type = LED_WAITING_RECORD;
			//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		}

#if C_MOTION_DETECTION == CUSTOM_ON
		if (state == STATE_MOTION_DETECTION) {
			OSTimeDly(100);
			if (!s_usbd_pin) {
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_MOTION_DETECT_START, NULL, NULL, MSG_PRI_NORMAL);
				ap_video_record_sts_set(VIDEO_RECORD_MD);
				led_type = LED_MOTION_DETECTION;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			}
		}
#endif
		return STATUS_OK;
	}
}

void ap_video_record_exit(void)
{	
#if C_MOTION_DETECTION == CUSTOM_ON
	ap_video_record_md_disable();
#endif
	//if (video_encode_status() == VIDEO_CODEC_PROCESSING) {
	if (video_record_sts & VIDEO_RECORD_BUSY) {

#if C_AUTO_DEL_FILE == CUSTOM_ON
		INT8U type = FALSE;
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT32U), MSG_PRI_NORMAL);
#endif

		if (video_encode_stop() != START_OK) {
			close(curr_file_info.file_handle);
			unlink((CHAR *) curr_file_info.file_path_addr);
			DBG_PRINT("Video Record Stop Fail\r\n");
			//return STATUS_FAIL;
		}
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
		DBG_PRINT("Video Record Stop\r\n");
		//chdir("C:\\DCIM");
	}

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
	if (cyclic_record_timerid != 0xFF) {
		sys_kill_timer(cyclic_record_timerid);
		cyclic_record_timerid = 0xFF;
	}
#endif

	video_record_sts = 0;
}

void ap_video_record_sts_set(INT8S sts)
{
	if (sts > 0) {
		video_record_sts |= sts;
	} else {
		video_record_sts &= sts;
	}
}

#if C_MOTION_DETECTION == CUSTOM_ON
void ap_video_record_md_tick(INT8U *md_tick,INT32U state)
{
	++*md_tick;
	if (*md_tick > MD_STOP_TIME) {
		*md_tick = 0;
		if (motion_detect_timerid != 0xFF) {
			sys_kill_timer(motion_detect_timerid);
			motion_detect_timerid = 0xFF;
		}
		ap_video_record_error_handle(state);
	}
}

INT8S ap_video_record_md_active(INT8U *md_tick,INT32U state)
{
	*md_tick = 0;
	if (video_record_sts & VIDEO_RECORD_UNMOUNT) {
		return STATUS_FAIL;
	}
	if (!(video_record_sts & VIDEO_RECORD_BUSY)) {
		if (ap_video_record_func_key_active(state)) {
			return STATUS_FAIL;
		} else {
			if (motion_detect_timerid == 0xFF) {
				motion_detect_timerid = MOTION_DETECT_TIMER_ID;
				sys_set_timer((void*)msgQSend, (void*)ApQ, MSG_APQ_MOTION_DETECT_TICK, motion_detect_timerid, MOTION_DETECT_CHECK_TIME_INTERVAL);
			}
		}
	}
	return STATUS_OK;
}

void ap_video_record_md_disable(void)
{
	if (motion_detect_timerid != 0xFF) {
		sys_kill_timer(motion_detect_timerid);
		motion_detect_timerid = 0xFF;
	}
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_MOTION_DETECT_STOP, NULL, NULL, MSG_PRI_NORMAL);
	ap_video_record_sts_set(~VIDEO_RECORD_MD);
}
#endif

void ap_video_record_usb_handle(void)
{
#if C_AUTO_DEL_FILE == CUSTOM_ON
		INT8U type = FALSE;
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT32U), MSG_PRI_NORMAL);
#endif
	
	if (video_record_sts & VIDEO_RECORD_BUSY) {
		if (video_encode_stop() != START_OK) {
			close(curr_file_info.file_handle);
			unlink((CHAR *) curr_file_info.file_path_addr);
			DBG_PRINT("Video Record Stop Fail\r\n");
		}

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
		if (cyclic_record_timerid != 0xFF) {
			sys_kill_timer(cyclic_record_timerid);
			cyclic_record_timerid = 0xFF;
		}
#endif		

		ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
	}
}
extern void led_red_on(void);
volatile INT8U video_down_flag=0;
INT8S ap_video_record_func_key_active(INT32U state)
{
	INT32U led_type;
	video_down_flag=1;

	if (video_record_sts & VIDEO_RECORD_BUSY)
	{
	led_red_on();
#if C_AUTO_DEL_FILE == CUSTOM_ON
		INT8U type = FALSE;
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT32U), MSG_PRI_NORMAL);
#endif

		if (video_encode_stop() != START_OK) {
			close(curr_file_info.file_handle);
			unlink((CHAR *) curr_file_info.file_path_addr);
			DBG_PRINT("Video Record Stop Fail\r\n");
			//return STATUS_FAIL;
		}

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
		else
		{
			if(g_lock_current_file_flag > 0)
			{
				_setfattr((CHAR *) curr_file_info.file_path_addr, D_RDONLY);
				g_lock_current_file_flag = 0;
			}
		}

		if (cyclic_record_timerid != 0xFF)
		{
			sys_kill_timer(cyclic_record_timerid);
			cyclic_record_timerid = 0xFF;
		}
#endif

		ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
		if (state == STATE_VIDEO_RECORD){
			led_type = LED_WAITING_RECORD;
		}
		else
		{
			led_type = LED_MOTION_DETECTION;
		}
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		DBG_PRINT("Video Record Stop\r\n");
		//chdir("C:\\DCIM");
		ap_state_handling_auto_power_off_set(TRUE);
		video_down_flag=0;
		return STATUS_FAIL;
	} else {
		if (video_record_sts == 0 || video_record_sts == VIDEO_RECORD_MD) {
			ap_video_record_sts_set(VIDEO_RECORD_BUSY + VIDEO_RECORD_WAIT);
			//led_type = LED_RECORD;
			//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_VID_REQ, NULL, NULL, MSG_PRI_NORMAL);
			OSTimeDly(1);
			ap_state_handling_auto_power_off_set(FALSE);
		} else {
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
			video_down_flag=0;
			return STATUS_FAIL;
		}
	}
	video_down_flag=0;
	return STATUS_OK;
}

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
void ap_video_record_cycle_reply_action(STOR_SERV_FILEINFO *file_info_ptr)
{
	MEDIA_SOURCE src;
	INT32U led_type;

	//ap_video_record_sts_set(~VIDEO_RECORD_WAIT);

	if (file_info_ptr->file_handle >= 0) 
	{
		gp_memcpy((INT8S *) &next_file_info, (INT8S *) file_info_ptr, sizeof(STOR_SERV_FILEINFO));

		if (next_file_info.file_handle >= 0) 
		{
		  #if C_AUTO_DEL_FILE == CUSTOM_ON
			INT8U type;

			type = FALSE;
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT8U), MSG_PRI_URGENT);
		  #endif

			src.type_ID.FileHandle = next_file_info.file_handle;
			src.type = SOURCE_TYPE_FS;
			src.Format.VideoFormat = MJPEG;

			if(video_encode_start(src) != START_OK)
			{
				close(src.type_ID.FileHandle);
				next_file_info.file_handle = -1;
				led_type = LED_CARD_NO_SPACE;//LED_SDC_FULL;
		        msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
				ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
				DBG_PRINT("jh_dbg_0506_7\r\n");

				//DBG_PRINT("Video Record Fail1\r\n");
				return;
			}
			else 
			{
				gp_memcpy((INT8S *)curr_file_info.file_path_addr, (INT8S *)next_file_info.file_path_addr, 24);
				curr_file_info.file_handle = next_file_info.file_handle;
			}
			next_file_info.file_handle = -1;

		  #if C_AUTO_DEL_FILE == CUSTOM_ON
			type = TRUE;
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT8U), MSG_PRI_NORMAL);
		  #endif
		}
	}
	else 
	{
	DBG_PRINT("jh_dbg_0506_8\r\n");
		//DBG_PRINT("Cyclic video record open file Fail\r\n");
	}
}
#endif

void ap_video_record_error_handle(INT32U state)
{
	if (video_record_sts & VIDEO_RECORD_BUSY) {
		ap_video_record_func_key_active(state);
	}
}

INT8U ap_video_record_sts_get(void)
{
	return video_record_sts;
}

void ap_video_record_reply_action(STOR_SERV_FILEINFO *file_info_ptr)
{
	MEDIA_SOURCE src;
	INT32U led_type;

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
	INT32U time_interval;// value;
	
	/*value = 0;//ap_state_config_record_time_get();
	if (value == 0) {
		time_interval = VIDEO_RECORD_CYCLE_TIME_INTERVAL*1;
	} else if (value == 1) {
		time_interval = VIDEO_RECORD_CYCLE_TIME_INTERVAL*5;
	} else {
		time_interval = VIDEO_RECORD_CYCLE_TIME_INTERVAL*10;
	}*/
    {
        bkground_del_disable(0);  // back ground auto delete enable
        time_interval = 5 * VIDEO_RECORD_CYCLE_TIME_INTERVAL + 112;  // dominant add 64 to record more
    }
#endif	

	if (file_info_ptr) {
		gp_memcpy((INT8S *) &curr_file_info, (INT8S *) file_info_ptr, sizeof(STOR_SERV_FILEINFO));
	} else {
		curr_file_info.storage_free_size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE) >> 20;
	}
	src.type_ID.FileHandle = curr_file_info.file_handle;
	src.type = SOURCE_TYPE_FS;
	src.Format.VideoFormat = MJPEG;
	if ((src.type_ID.FileHandle >=0) && (curr_file_info.storage_free_size > REMAIN_SPACE))
	{
		if (video_encode_start(src) != START_OK) {
			close(src.type_ID.FileHandle);
			unlink((CHAR *) curr_file_info.file_path_addr);
			led_type = LED_CARD_NO_SPACE;//LED_SDC_FULL;
	        msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
			//DBG_PRINT("Video Record Fail\r\n");
		} else {
		     led_type = LED_RECORD;
			 msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
			if (cyclic_record_timerid == 0xFF) {
				cyclic_record_timerid = VIDEO_RECORD_CYCLE_TIMER_ID;
				sys_set_timer((void*)msgQSend, (void*)ApQ, MSG_APQ_CYCLIC_VIDEO_RECORD, cyclic_record_timerid, time_interval);
			}
#endif

#if C_AUTO_DEL_FILE == CUSTOM_ON
			{
				INT8U type = TRUE;
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT32U), MSG_PRI_NORMAL);
			}
#endif
		}
	} else {
		close(src.type_ID.FileHandle);
		unlink((CHAR *) curr_file_info.file_path_addr);
		DBG_PRINT("Video Record Stop [NO free space]\r\n");
		ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
		led_type =LED_CARD_NO_SPACE;//LED_CARD_NO_SPACE;// LED_SDC_FULL;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
		//msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
	}
}
