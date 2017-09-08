#include "state_video_record.h"

//	prototypes
INT8S	state_video_record_init(INT32U state);
void	state_video_record_exit(void);

INT8S state_video_record_init(INT32U state)
{
	DBG_PRINT("video_record state init enter\r\n");

	if (ap_video_record_init(state) == STATUS_OK) {
		return STATUS_OK;
	} else {
		return STATUS_FAIL;
	}
}

void state_video_record_entry(void *para, INT32U state)
{
	EXIT_FLAG_ENUM exit_flag;
	INT32U msg_id, led_type;
    INT32U disk_free_size;
	INT8U send_active=0;

#if C_MOTION_DETECTION == CUSTOM_ON
	INT8U md_check_tick = 0, switch_flag = 0;
#endif

	if (state_video_record_init(state) == STATUS_OK) {
		if (*(INT32U *) para != STATE_AUDIO_RECORD) {
		
			OSTimeDly(10);
			if (!s_usbd_pin) {
				//ap_video_record_func_key_active(state);
			}
			if(*(INT32U *) para==STATE_VIDEO_PREVIEW)
			msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
		}
		exit_flag = EXIT_RESUME;
	} 
	else
	{
		//OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
		//exit_flag = EXIT_BREAK;
		exit_flag = EXIT_RESUME;
	}

	while (exit_flag == EXIT_RESUME) {
		if (msgQReceive(ApQ, &msg_id, (void *) ApQ_para, AP_QUEUE_MSG_MAX_LEN) == STATUS_FAIL) {
			continue;
		}

		switch (msg_id) {
			case MSG_STORAGE_SERVICE_MOUNT:
        		ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_video_record_sts_set(~VIDEO_RECORD_UNMOUNT);

        		DBG_PRINT("[Video Record Mount OK]\r\n");
        		break;

        	case MSG_STORAGE_SERVICE_NO_STORAGE:
        		ap_state_handling_storage_id_set(ApQ_para[0]);
        		ap_video_record_sts_set(VIDEO_RECORD_UNMOUNT);

        		DBG_PRINT("[Video Record Mount FAIL]\r\n");
        		break;

#if C_MOTION_DETECTION == CUSTOM_ON
			case MSG_APQ_MOTION_DETECT_TICK:
				ap_video_record_md_tick(&md_check_tick,state);
				break;

			case MSG_APQ_MOTION_DETECT_ACTIVE:
				if (ap_video_record_md_active(&md_check_tick,state)) {
					OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
					exit_flag = EXIT_BREAK;
				}
				break;
#endif

            case MSG_APQ_VIDEO_RECORD_ACTIVE:
				if (ap_video_record_sts_get() & VIDEO_RECORD_WAIT) {
					OSTimeDly(2);
					msgQSend(ApQ, msg_id, NULL, NULL, MSG_PRI_NORMAL);
					break;
				}

#if C_MOTION_DETECTION == CUSTOM_ON
        		if (video_encode_status() == VIDEO_CODEC_PROCESSING) {
        			ap_video_record_md_disable();
        		}
#endif
	        	ap_video_record_func_key_active(state);
        		break;

            case  MSG_APQ_AUDIO_REC_ACTIVE:
				if (ap_video_record_sts_get() & VIDEO_RECORD_WAIT) {
					OSTimeDly(2);
					msgQSend(ApQ, msg_id, NULL, NULL, MSG_PRI_NORMAL);
					break;
				}

				if(ap_video_record_sts_get() & VIDEO_RECORD_BUSY) {
					ap_video_record_func_key_active(state);
				}  	
				OSQPost(StateHandlingQ, (void *) STATE_AUDIO_RECORD);
				exit_flag = EXIT_BREAK;
				break;

        	case MSG_APQ_CAPTUER_ACTIVE:
				if (ap_video_record_sts_get() & VIDEO_RECORD_WAIT) {
					OSTimeDly(2);
					msgQSend(ApQ, msg_id, NULL, NULL, MSG_PRI_NORMAL);
					break;
				}
				if(ap_video_record_sts_get() & VIDEO_RECORD_BUSY) {
					send_active=0;
	                #if CAPTURE_KEY_SAVE_VIDEO == 1
					ap_video_record_func_key_active(state);
					#endif
					break;
				}
				else
					send_active=1;

				OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
				if(send_active)
        		  msgQSend(ApQ, MSG_APQ_CAPTUER_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
                else
				{
				   //led_type = LED_WAITING_CAPTURE;
				   //msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
				}
				exit_flag = EXIT_BREAK;
				break;

			case MSG_APQ_MOTION_DETECT_START:
				if (ap_video_record_sts_get() & VIDEO_RECORD_WAIT) {
					OSTimeDly(2);
					msgQSend(ApQ, msg_id, NULL, NULL, MSG_PRI_NORMAL);
					break;
				}

				if(ap_video_record_sts_get() & VIDEO_RECORD_BUSY) {
					ap_video_record_func_key_active(state);
				}
				OSQPost(StateHandlingQ, (void *) STATE_MOTION_DETECTION);
				exit_flag = EXIT_BREAK;
				break;

        	case MSG_STORAGE_SERVICE_VID_REPLY:
				ap_video_record_sts_set(~VIDEO_RECORD_WAIT);
        		ap_video_record_reply_action((STOR_SERV_FILEINFO *) ApQ_para);
#if C_MOTION_DETECTION == CUSTOM_ON
        		switch_flag = 0;
#endif
        		break;

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
			case MSG_STORAGE_SERVICE_VID_CYCLIC_REPLY:
				ap_video_record_sts_set(~VIDEO_RECORD_WAIT);
        		ap_video_record_cycle_reply_action((STOR_SERV_FILEINFO *) ApQ_para);
        		break;

        	case MSG_APQ_CYCLIC_VIDEO_RECORD:
#if C_AUTO_DEL_FILE == CUSTOM_ON
        		msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_AUTO_DEL_LOCK, NULL, NULL, MSG_PRI_NORMAL);
#endif
				video_encode_stop();
				ap_video_record_sts_set(VIDEO_RECORD_WAIT);
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_VID_CYCLIC_REQ, NULL, NULL, MSG_PRI_NORMAL);
        		break;
#endif

			case MSG_APQ_CONNECT_TO_PC:
				if (ap_video_record_sts_get() & VIDEO_RECORD_WAIT) {
					OSTimeDly(2);
					msgQSend(ApQ, msg_id, NULL, NULL, MSG_PRI_NORMAL);
					break;
				}

        		ap_video_record_usb_handle();
        		led_type = LED_USB_CONNECT;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
        		ap_state_handling_connect_to_pc(ApQ_para[0]);        		
        		break;

        	case MSG_APQ_DISCONNECT_TO_PC:
        		ap_state_handling_disconnect_to_pc();
        		OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
				exit_flag = EXIT_BREAK;
        		break;

        	case MSG_APQ_RECORD_SWITCH_FILE:
#if C_MOTION_DETECTION == CUSTOM_ON	
        		if (!switch_flag) {
	        		switch_flag = 1;
	        		ap_video_record_func_key_active(state);
	        		if (ap_video_record_func_key_active(state)) {
	        			OSQPost(StateHandlingQ, (void *) STATE_VIDEO_PREVIEW);
	        			exit_flag = EXIT_BREAK;
	        		}
	        	}
#endif
        		break;

            case MSG_APQ_VDO_REC_STOP:
				ap_video_record_error_handle(state);
				ap_video_record_sts_set(~VIDEO_RECORD_BUSY);
				disk_free_size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE) >> 20;
				if (disk_free_size<=10) {
					//ap_state_handling_auto_power_off_handle();
					//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_FLASH_STORAGE_FULL, NULL, NULL, MSG_PRI_NORMAL);
                led_type = LED_CARD_NO_SPACE;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
				}
                break;             	
            	
        	case MSG_APQ_AVI_PACKER_ERROR:
				if (ap_video_record_sts_get() & VIDEO_RECORD_BUSY) {
					ap_video_record_func_key_active(state);
				}

				if(ApQ_para[0])
				{
					INT16S nRet;

                    nRet = _devicemount(MINI_DVR_STORAGE_TYPE);
		            if(FatSpeedUp_Array[MINI_DVR_STORAGE_TYPE].UsedCnt == 0)
		            {
		                if(gSelect_Size == 5*1024*1024/512)   // all cluster groups of free space are smaller than 5M, not 500KB yet
		                {
		                    gSelect_Size = 500*1024/512;
			                nRet = _devicemount(MINI_DVR_STORAGE_TYPE);
			                if(FatSpeedUp_Array[MINI_DVR_STORAGE_TYPE].UsedCnt == 0)
			                {
			                    gOutOfFreeSpace = 1;
			                }
		                }
						else // all cluster groups of free space are smaller than 500KB
						{
						    gOutOfFreeSpace = 1;
						}
		            }
					ap_video_record_func_key_active(state);
				}
				else
				{
	        		ap_state_handling_auto_power_off_handle();
        		}
        		break;

        	case MSG_APQ_POWER_KEY_FLASH_ACTIVE:
        	case MSG_APQ_POWER_KEY_ACTIVE:
        		ap_video_record_error_handle(state);
        		//ap_state_handling_power_off_handle(msg_id);
        		break;
        	case MSG_APQ_AUTO_POWER_OFF:
        		if ((ap_video_record_sts_get() & VIDEO_RECORD_BUSY) == 0) {
        			ap_state_handling_auto_power_off_handle();
        		}
        		break;

        	case MSG_APQ_LOW_BATTERY_HANDLE:
        		ap_video_record_error_handle(state);
        		ap_state_handling_auto_power_off_handle();
        		break;

			default:
				break;
		}
	}

	if (exit_flag == EXIT_BREAK) {
		state_video_record_exit();
	}
}

void state_video_record_exit(void)
{
	ap_video_record_exit();
	ap_state_handling_auto_power_off_set(TRUE);
	DBG_PRINT("Exit video_record state\r\n");
}