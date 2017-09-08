#include "task_storage_service.h"
#include "secrecy.h"

MSG_Q_ID StorageServiceQ;
void *storage_service_q_stack[STORAGE_SERVICE_QUEUE_MAX];
static INT8U storage_service_para[STORAGE_SERVICE_QUEUE_MAX_MSG_LEN];

//	prototypes
void task_storage_service_init(void);

void task_storage_service_init(void)
{
	StorageServiceQ = msgQCreate(STORAGE_SERVICE_QUEUE_MAX, STORAGE_SERVICE_QUEUE_MAX, STORAGE_SERVICE_QUEUE_MAX_MSG_LEN);
	ap_storage_service_init();
}

void task_storage_service_entry(void *para)
{
	INT32U msg_id;
DBG_PRINT("llll\r\n");
	task_storage_service_init();
	
	while(1) {	
		if (msgQReceive(StorageServiceQ, &msg_id, storage_service_para, STORAGE_SERVICE_QUEUE_MAX_MSG_LEN) == STATUS_FAIL) {
			continue;
		}		
        switch (msg_id) {
        	case MSG_STORAGE_SERVICE_STORAGE_CHECK:
				DBG_PRINT("mount\r\n");
        		if (!s_usbd_pin) {
        			ap_storage_service_storage_mount();
        		}
        		break;
        	case MSG_STORAGE_SERVICE_USB_IN:
        		ap_storage_service_usb_plug_in();
        		break;
#if C_AUTO_DEL_FILE == CUSTOM_ON
        	case MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH:
        		ap_storage_service_freesize_check_switch(storage_service_para[0]);
        		break;
        	case MSG_STORAGE_SERVICE_FREESIZE_CHECK:
        		ap_storage_service_freesize_check_and_del();
        		break;
        	case MSG_STORAGE_SERVICE_AUTO_DEL_LOCK:
        		ap_storage_service_freesize_check_switch(FALSE);
        		break;
#endif
        	case MSG_STORAGE_SERVICE_TIMER_START:
        		ap_storage_service_timer_start();
        		break;
        	case MSG_STORAGE_SERVICE_TIMER_STOP:
        		ap_storage_service_timer_stop();
        		break;
        	case MSG_STORAGE_SERVICE_AUD_REQ:
        	case MSG_STORAGE_SERVICE_PIC_REQ:
        	case MSG_STORAGE_SERVICE_VID_REQ:
        		ap_storage_service_file_open_handle(msg_id);
        		break;
#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
			case MSG_STORAGE_SERVICE_VID_CYCLIC_REQ:
				ap_storage_service_cyclic_record_file_open_handle();
				break;
#endif       
#if SECRECY_ENABLE
			case MSG_STORAGE_SERVICE_SECU_READ:
				Secrecy_Function();
				break;
#endif
        	default:
        		break;
        }
        
    }
}