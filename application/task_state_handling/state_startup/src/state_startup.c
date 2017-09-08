#include "state_startup.h"

//	prototypes
void state_startup_init(void);
void state_startup_exit(void);

void state_startup_init(void)
{
	DBG_PRINT("Startup state init enter\r\n");
	ap_startup_init();
	R_SYSTEM_CTRL |= 0x30;	//27M => weak mode : |= 0x30
	R_SYSTEM_CLK_CTRL |= 0x1000;	//32768	=> weak mode : |= 0x1000
}

void state_startup_entry(void *para)
{
	EXIT_FLAG_ENUM exit_flag = EXIT_RESUME;
	INT32U msg_id;
	INT8U flag1, flag2;
	
	flag1 = flag2 = 0;
	
	while (exit_flag == EXIT_RESUME) {
		if (msgQReceive(ApQ, &msg_id, (void *) ApQ_para, AP_QUEUE_MSG_MAX_LEN) == STATUS_FAIL) {
			continue;
		}
		switch (msg_id) {
			case MSG_STORAGE_SERVICE_MOUNT:
				ap_umi_firmware_upgrade();
			case MSG_STORAGE_SERVICE_NO_STORAGE:
				ap_state_handling_storage_id_set(ApQ_para[0]);
				flag1 = 1;
        		break;
			case MSG_APQ_PERIPHERAL_TASK_READY:
				flag2 = 1;
				break;
			case MSG_APQ_POWER_KEY_FLASH_ACTIVE:
			case MSG_APQ_POWER_KEY_ACTIVE:
        		//if (flag2 == 2) {
        		//	ap_state_handling_power_off_handle(msg_id);
        		//}
        		break;
			case MSG_APQ_LOW_BATTERY_HANDLE:
        		flag2 = 2;
        		ap_state_handling_auto_power_off_handle();
        		break;
			default:
				break;
		}		
		if (flag1 == 1 && flag2 == 1) {
			DBG_PRINT("AA\r\n");
			exit_flag = EXIT_BREAK;
		} else {
		DBG_PRINT("BB\r\n");
			exit_flag = EXIT_RESUME;
		}
	}
	//ap_state_firmware_upgrade();
	state_startup_init();
	state_startup_exit();
}

void state_startup_exit(void)
{
    DBG_PRINT("Exit Startup state\r\n");
    msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_USBD_DETECT_INIT, NULL, NULL, MSG_PRI_NORMAL);
    OSQPost(StateHandlingQ, (void *) STATE_VIDEO_RECORD);
}