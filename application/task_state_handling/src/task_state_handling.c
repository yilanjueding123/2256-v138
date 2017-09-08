#include "task_state_handling.h"

#define STATE_HANDLING_QUEUE_MAX			5
#define AP_QUEUE_MAX    					128

MSG_Q_ID ApQ;
OS_EVENT *StateHandlingQ;
INT8U ApQ_para[AP_QUEUE_MSG_MAX_LEN];
void *state_handling_q_stack[STATE_HANDLING_QUEUE_MAX];

//	prototypes
void state_handling_init(void);

void state_handling_init(void)
{
//	INT32S config_load_flag;
	
	StateHandlingQ = OSQCreate(state_handling_q_stack, STATE_HANDLING_QUEUE_MAX);
	ApQ = msgQCreate(AP_QUEUE_MAX, AP_QUEUE_MAX, AP_QUEUE_MSG_MAX_LEN);
	ap_state_handling_storage_id_set(NO_STORAGE);
	
//	nvmemory_init();
//	OSTimeDly(1);
//	config_load_flag = ap_state_config_load();
//	ap_state_resource_init();
//	ap_state_config_initial(config_load_flag);
//ap_state_config_initial(STATUS_FAIL);
//	ap_state_handling_calendar_init();
}

void state_handling_entry(void *para)
{
	INT32U msg_id, prev_state;
	INT8U err;
	
	msg_id = 0;
	state_handling_init();
	OSQPost(StateHandlingQ, (void *) STATE_STARTUP);
	
	while(1) {
		prev_state = msg_id;
		msg_id = (INT32U) OSQPend(StateHandlingQ, 0, &err);
		if((!msg_id) || (err != OS_NO_ERR)) {
        	continue;
        }
		switch(msg_id) {
			case STATE_STARTUP:
				state_startup_entry((void *) &prev_state);
				break;
			case STATE_VIDEO_PREVIEW:
				state_video_preview_entry((void *) &prev_state);
				break;
#if C_MOTION_DETECTION == CUSTOM_ON			
			case STATE_MOTION_DETECTION:
#endif			
			case STATE_VIDEO_RECORD:
				state_video_record_entry((void *) &prev_state, msg_id);
				break;
			case STATE_AUDIO_RECORD:
				state_audio_record_entry((void *) &prev_state);
				break;
			case STATE_BROWSE:
	//			state_browse_entry((void *) &prev_state);
				break;
			case STATE_SETTING:
	//			state_setting_entry((void *) &prev_state);
				break;
			default:
				break;
		}
	}
}