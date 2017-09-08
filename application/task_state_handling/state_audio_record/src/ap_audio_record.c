#include "ap_audio_record.h"

static INT8S audio_record_sts;

INT8S ap_audio_record_init(void)
{	
	if (ap_state_handling_storage_id_get() == NO_STORAGE) {
		ap_audio_record_sts_set(AUDIO_RECORD_UNMOUNT);
		return STATUS_FAIL;
	} else {
		ap_audio_record_sts_set(~AUDIO_RECORD_UNMOUNT);
		return STATUS_OK;
	}
}

void ap_audio_record_exit(void)
{
	if (audio_record_sts & AUDIO_RECORD_BUSY) {
		audio_encode_stop();
	}
	audio_record_sts = 0;
}

void ap_audio_record_sts_set(INT8S sts)
{
	if (sts > 0) {
		audio_record_sts |= sts;
	} else {
		audio_record_sts &= sts;
	}
}

INT8U ap_audio_record_sts_get(void)
{
	if (audio_record_sts & AUDIO_RECORD_BUSY) {
		return 0;
	} else {
		return 1;
	}
}

void ap_audio_record_error_handle(void)
{
	if (audio_record_sts & AUDIO_RECORD_BUSY) {
		ap_audio_record_func_key_active();
	}
}

void ap_audio_record_func_key_active(void)
{
	INT32U led_type;
	if (audio_record_sts == 0) {
		ap_state_handling_auto_power_off_set(FALSE);
		ap_audio_record_sts_set(AUDIO_RECORD_BUSY);
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_AUD_REQ, NULL, NULL, MSG_PRI_NORMAL);
		led_type = LED_AUDIO_RECORD;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);

	} else if (audio_record_sts & AUDIO_RECORD_BUSY) {
		ap_state_handling_auto_power_off_set(TRUE);
		audio_encode_stop();
		led_type = LED_WAITING_AUDIO_RECORD;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
		ap_audio_record_sts_set(~AUDIO_RECORD_BUSY);
	}
}

INT8S ap_audio_record_reply_action(STOR_SERV_FILEINFO *file_info_ptr)
{
	MEDIA_SOURCE src;	
	INT32U file_path_addr = file_info_ptr->file_path_addr;
	INT16S free_size = file_info_ptr->storage_free_size;
	INT32U BitRate;
	
	src.type_ID.FileHandle = file_info_ptr->file_handle;
	src.type = SOURCE_TYPE_FS;
	if(AUD_REC_FORMAT == AUD_FORMAT_WAV)
	  {
		  src.Format.AudioFormat = WAV;
		  BitRate =0;
	  }
	 else{
	      src.Format.AudioFormat = MP3;
		  BitRate =128000;
	       }
	if (src.type_ID.FileHandle >=0 && free_size > REMAIN_SPACE) {
		if (audio_encode_start(src, AUD_REC_SAMPLING_RATE, BitRate)) {
			return STATUS_FAIL;
		}
	} else {
		if (src.type_ID.FileHandle >= 0) {
			close(src.type_ID.FileHandle);
			unlink((CHAR *) file_path_addr);
		}
		DBG_PRINT("[POWER OFF event sent(audio record)]\r\n");
		msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
	}
	return STATUS_OK;
}