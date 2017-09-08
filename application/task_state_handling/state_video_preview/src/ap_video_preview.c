#include "ap_video_preview.h"

static INT8S video_preview_sts;

void ap_video_preview_init(void)
{	
	//OSQPost(DisplayTaskQ, (void *) MSG_DISPLAY_TASK_QUEUE_INIT);
	//OSQPost(scaler_task_q, (void *) MSG_SCALER_TASK_PREVIEW_ON);	
	if (ap_state_handling_storage_id_get() == NO_STORAGE) {
		ap_video_preview_sts_set(VIDEO_PREVIEW_UNMOUNT);
	} else {
		ap_video_preview_sts_set(~VIDEO_PREVIEW_UNMOUNT);
	}
	
}

void ap_video_preview_exit(void)
{
	video_preview_sts = 0;
}

void ap_video_preview_sts_set(INT8S sts)
{
	if (sts > 0) {
		video_preview_sts |= sts;
	} else {
		video_preview_sts &= sts;
	}
}

INT8U ap_video_preview_sts_get(void)
{
	if (video_preview_sts & VIDEO_PREVIEW_BUSY) {
		return 0;
	} else {
		return 1;
	}
}

void ap_video_preview_func_key_active(void)
{
	//INT32U led_type;
	
	if (video_preview_sts == 0) {
		ap_video_preview_sts_set(VIDEO_PREVIEW_BUSY);
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_PIC_REQ, NULL, NULL, MSG_PRI_NORMAL);
		//led_type = LED_CAPTURE;
		//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
	}
}

INT32S ap_video_preview_reply_action(STOR_SERV_FILEINFO *file_info_ptr)
{
	MEDIA_SOURCE src;
	INT32U file_path_addr = file_info_ptr->file_path_addr;
	INT16S free_size = file_info_ptr->storage_free_size;
	INT32U led_type;
	src.type_ID.FileHandle = file_info_ptr->file_handle;
	src.type = SOURCE_TYPE_FS;
	src.Format.VideoFormat = MJPEG;

	if ((src.type_ID.FileHandle >=0) && (free_size > REMAIN_SPACE)) {

		if (video_encode_capture_picture(src) != START_OK) {
			unlink((CHAR *) file_path_addr);
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
			led_type =LED_CAPTURE_FAIL;
			msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			DBG_PRINT("jh_dbg_0506_1\r\n");
			return STATUS_FAIL;
		}
		else
			{
			led_type = LED_CAPTURE;
		    msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			return STATUS_OK;
			}
	} else {
		if (src.type_ID.FileHandle >=0) {
			close(src.type_ID.FileHandle);
			unlink((CHAR *) file_path_addr);
			led_type =LED_CARD_NO_SPACE;//LED_CARD_NO_SPACE;//LED_CARD_NO_SPACE;// LED_SDC_FULL;
			msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			led_type =LED_CARD_TELL_FULL;//LED_CARD_NO_SPACE;//LED_CARD_NO_SPACE;// LED_SDC_FULL;
			msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			DBG_PRINT("jh_dbg_0506_2\r\n");
		}
		else
		{
			 led_type =LED_CAPTURE_FAIL;
			 msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			 DBG_PRINT("jh_dbg_0506_3\r\n");
		}
	
        msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
		return	STATUS_FAIL;
	}
}

void ap_video_preview_reply_done(INT8U ret, INT32U file_path_addr)
{
	if (!ret) {
		DBG_PRINT("Capture OK\r\n");
	} else {
		unlink((CHAR *) file_path_addr);
		DBG_PRINT("Capture Fail\r\n");
	}
	ap_video_preview_sts_set(~VIDEO_PREVIEW_BUSY);
	msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
}
