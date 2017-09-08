#include "state_video_preview.h"

INT8U capture_ready=0;
//	prototypes
void state_video_preview_init(void);
void state_video_preview_exit(void);

void state_video_preview_init(void)
{
	DBG_PRINT("video_preview state init enter\r\n");
	 capture_ready=1;
	ap_video_preview_init();
//	OSTimeDly(50);

}
volatile INT8U pic_down_flag=0;
#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
extern void capture_switch(void);
#endif
void state_video_preview_entry(void *para)
{
	EXIT_FLAG_ENUM exit_flag = EXIT_RESUME;
	INT32U msg_id, file_path_addr;
	INT32U led_type;
	INT8U pic_flag;
#if C_MOTION_DETECTION == CUSTOM_ON
	INT8U md_key_cnt, md_timerid;

	md_key_cnt = 0;
	md_timerid = 0xFF;
#endif		
	pic_flag = 0;


//	led_type = LED_WAITING_CAPTURE;
//	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
	state_video_preview_init();


	while (exit_flag == EXIT_RESUME) {
		if (msgQReceive(ApQ, &msg_id, (void *) ApQ_para, AP_QUEUE_MSG_MAX_LEN) == STATUS_FAIL) {
			continue;
		}

		switch (msg_id) {
			case MSG_STORAGE_SERVICE_MOUNT:
				ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_video_preview_sts_set(~VIDEO_PREVIEW_UNMOUNT);
        		DBG_PRINT("[Video Preview Mount OK]\r\n");
        		break;
        	case MSG_STORAGE_SERVICE_NO_STORAGE:
				//led_type = LED_NO_SDC;
				//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
        		ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_video_preview_sts_set(VIDEO_PREVIEW_UNMOUNT);

        		DBG_PRINT("[Video Preview Mount FAIL]\r\n");
        		break;
        	case MSG_APQ_VIDEO_RECORD_ACTIVE:
        		if (ap_video_preview_sts_get()) {      		
	        		OSQPost(StateHandlingQ, (void *) STATE_VIDEO_RECORD);
	        		exit_flag = EXIT_BREAK;
                }
        		break;
            case  MSG_APQ_AUDIO_REC_ACTIVE:	
				if (ap_video_preview_sts_get()) { 	
					OSQPost(StateHandlingQ, (void *) STATE_AUDIO_RECORD);
					exit_flag = EXIT_BREAK;
				}
				break;

        	case MSG_APQ_CAPTUER_ACTIVE:
				
				__msg("MSG_APQ_CAPTUER_ACTIVE\n");
				pic_flag = 1;
				ap_state_handling_auto_power_off_set(FALSE);
				pic_down_flag=1;
				ap_video_preview_func_key_active();
				break;

			case MSG_APQ_MOTION_DETECT_START:
				OSQPost(StateHandlingQ, (void *) STATE_MOTION_DETECTION);
                exit_flag = EXIT_BREAK;
				break;

        	case MSG_STORAGE_SERVICE_PIC_REPLY:
        		//ap_video_preview_reply_action((STOR_SERV_FILEINFO *) ApQ_para);
        		if(ap_video_preview_reply_action((STOR_SERV_FILEINFO *) ApQ_para) < 0) 
				{
        			pic_flag = 0;
					pic_down_flag=0;
        			break;
        		}
        		file_path_addr = ((STOR_SERV_FILEINFO *) ApQ_para)->file_path_addr;
        		//OSTimeDly(35);
				//led_type = LED_WAITING_CAPTURE;
			    //msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);

        		break;
        	case MSG_STORAGE_SERVICE_PIC_DONE:
        		ap_video_preview_reply_done(ApQ_para[0], file_path_addr);
				#if (PIC_WIDTH == 2560) && (PIC_HEIGH == 1440)
        		capture_switch();
        		#else
        		OSTimeDly(35);
				#endif
				__msg(">>>>MSG_STORAGE_SERVICE_PIC_DONE<<<<<\n");
				pic_down_flag=0;
        		ap_state_handling_auto_power_off_set(TRUE);
        		if (pic_flag == 2) {
        			msgQSend(ApQ, MSG_APQ_CONNECT_TO_PC, &pic_flag, sizeof(INT8U), MSG_PRI_NORMAL);
        		}
        		pic_flag = 0;
				led_type = LED_WAITING_CAPTURE;
			    msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
        		break;
        	case MSG_APQ_POWER_KEY_FLASH_ACTIVE:
        	case MSG_APQ_POWER_KEY_ACTIVE:
        		//ap_state_handling_power_off_handle(msg_id);
        		break;
        	case MSG_APQ_CONNECT_TO_PC:
        		if (!pic_flag) {
	        		led_type = LED_USB_CONNECT;
					msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
	        		ap_state_handling_connect_to_pc(ApQ_para[0]);
	        	} else {
	        		pic_flag = 2;
	        	}
        		break;
        	case MSG_APQ_DISCONNECT_TO_PC:
        		ap_state_handling_disconnect_to_pc();
        		led_type = LED_WAITING_CAPTURE;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
        		break;
			case MSG_APQ_AUTO_POWER_OFF:
        		ap_state_handling_auto_power_off_handle();
        		break;
        	case MSG_APQ_LOW_BATTERY_HANDLE:
        		ap_state_handling_auto_power_off_handle();
        		break;
			default:
				break;
		}
		
	}
	
	if (exit_flag == EXIT_BREAK) {
#if C_MOTION_DETECTION == CUSTOM_ON	//wwj add for OV7725
		if (md_timerid != 0xFF) {
    		sys_kill_timer(md_timerid);
			md_timerid = 0xFF;
		}
#endif
		state_video_preview_exit();
	}
}

void state_video_preview_exit(void)
{
	ap_video_preview_exit();
	capture_ready=0;
	DBG_PRINT("Exit video_preview state\r\n");
}
