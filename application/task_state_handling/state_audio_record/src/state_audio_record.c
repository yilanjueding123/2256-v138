#include "state_audio_record.h"

//	prototypes
INT8S state_audio_record_init(void);
void state_audio_record_exit(void);

INT8S state_audio_record_init(void)
{
	audio_encode_entrance();
	DBG_PRINT("audio_record state init enter\r\n");
	if (ap_audio_record_init() == STATUS_OK) {
		return STATUS_OK;
	} else {
		return STATUS_FAIL;
	}
}

void state_audio_record_entry(void *para)
{
	EXIT_FLAG_ENUM exit_flag = EXIT_RESUME;
	INT32U msg_id,led_type;
	INT8U switch_flag;
		
	switch_flag = 0;
	if (state_audio_record_init() == STATUS_OK) {
		exit_flag = EXIT_RESUME;
		led_type = LED_WAITING_AUDIO_RECORD;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		if (!s_usbd_pin) {
			msgQSend(ApQ, MSG_APQ_AUDIO_REC_ACTIVE,NULL,NULL, MSG_PRI_NORMAL);
		}
	} else {
		OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
		exit_flag = EXIT_BREAK;
	}
	while (exit_flag == EXIT_RESUME) {
		if (msgQReceive(ApQ, &msg_id, (void *) ApQ_para, AP_QUEUE_MSG_MAX_LEN) == STATUS_FAIL) {
			continue;
		}		
		switch (msg_id) {
			case MSG_STORAGE_SERVICE_MOUNT:
				ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_audio_record_sts_set(~AUDIO_RECORD_UNMOUNT);
        		DBG_PRINT("[Audio Record Mount OK]\r\n");
        		break;
        	case MSG_STORAGE_SERVICE_NO_STORAGE:
        		ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_audio_record_sts_set(AUDIO_RECORD_UNMOUNT);
        		DBG_PRINT("[Audio Record Mount FAIL]\r\n");
        		break;
        	case MSG_APQ_MODE:
        	    OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
        		exit_flag = EXIT_BREAK;
        		break;

           case MSG_APQ_AUDIO_REC_ACTIVE:
				ap_audio_record_func_key_active();
		//		OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
		//		exit_flag = EXIT_BREAK;
        		break;
        	case MSG_STORAGE_SERVICE_AUD_REPLY:
        		if (ap_audio_record_reply_action((STOR_SERV_FILEINFO *) ApQ_para)) {
        			OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
					exit_flag = EXIT_BREAK;
        		}
        		switch_flag = 0;
        		break;
           
            case MSG_APQ_CAPTUER_ACTIVE:
				if(!ap_audio_record_sts_get()){
				  ap_audio_record_func_key_active();
				}
				OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
	        	exit_flag = EXIT_BREAK;
        		break; 
            case MSG_APQ_VIDEO_RECORD_ACTIVE:
				if(!ap_audio_record_sts_get()){
				  ap_audio_record_func_key_active();
				}
				OSQPost(StateHandlingQ, (void *) STATE_VIDEO_RECORD);
	        	exit_flag = EXIT_BREAK;
        		break;
        	case MSG_APQ_RECORD_SWITCH_FILE:
        		if (!switch_flag) {
	        		switch_flag = 1;
	        		ap_audio_record_func_key_active();
	        		ap_audio_record_func_key_active();
	        	}
        		break;
        	case MSG_APQ_POWER_KEY_FLASH_ACTIVE:
        	case MSG_APQ_POWER_KEY_ACTIVE:
        		ap_audio_record_exit();
        		//ap_state_handling_power_off_handle(msg_id);
        		break;
        	case MSG_APQ_CONNECT_TO_PC:
        		if (!ap_audio_record_sts_get()) {
        			ap_audio_record_func_key_active();
        		}
        		led_type = LED_USB_CONNECT;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
        		ap_state_handling_connect_to_pc(ApQ_para[0]);
        		break;
        	case MSG_APQ_DISCONNECT_TO_PC:
        		ap_state_handling_disconnect_to_pc();
        		OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
				exit_flag = EXIT_BREAK;
        		break;
			case MSG_APQ_AUTO_POWER_OFF:
        		if (ap_audio_record_sts_get()) {
        			ap_state_handling_auto_power_off_handle();
        		}
        		break;
        	case MSG_APQ_AVI_PACKER_ERROR:
        		if (!ap_audio_record_sts_get()) {
        			ap_audio_record_exit();
        			ap_state_handling_auto_power_off_handle();
        		}
        		break;
        	case MSG_APQ_LOW_BATTERY_HANDLE:
        	    if (!ap_audio_record_sts_get()) {
        			ap_audio_record_exit();	
        		}
        		ap_state_handling_auto_power_off_handle();
        		break;
			default:
				break;
		}
		
	}
	
	if (exit_flag == EXIT_BREAK) {
		state_audio_record_exit();
	}
}

void state_audio_record_exit(void)
{
	ap_state_handling_auto_power_off_set(TRUE);
	ap_audio_record_exit();
	audio_encode_exit();
	DBG_PRINT("Exit audio_record state\r\n");
}