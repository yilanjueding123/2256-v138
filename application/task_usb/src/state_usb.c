
/* pseudo header include */
#include "state_usb.h"

#define USB_TASK_QUEUE_MAX	32

static INT8U usb_state_flag=0;//0:?TUSB ?T?e?¡ê  1:USB 2:?e?¡ê 3:usb or ?e?¡ê

OS_EVENT *USBTaskQ;
void *usb_task_q_stack[USB_TASK_QUEUE_MAX];
INT8U usbd_flag;
extern INT32U SendISOData;
extern INT32U SendAudioData;
extern INT8U s_usbd_pin;
extern INT8U capture_ready;
//void state_usb_entry(void* para1);
static void state_usb_init(void);
//static void state_usb_exit(void);
//static void state_usb_suspend(EXIT_FLAG_ENUM exit_flag);
extern void usb_msdc_state_exit(void);
void sys_usbd_state_flag_set(INT8U in_or_out);


void state_usb_init(void)
{
    sys_usbd_state_flag_set(C_USBD_STATE_IN);
	usb_msdc_state_initial();

}

INT8U usb_state_get(void)
{
 	return usb_state_flag;
}
void usb_state_set(INT8U flag)
{
 	usb_state_flag=flag;
}


void ap_usb_isr(void)
{
	INT8U type;
	if (*P_USBD_INT == 0x8000) {
		*P_USBD_INT |= 0x8000;
		vic_irq_disable(VIC_USB);
		usb_uninitial();
		s_usbd_pin = 1;
		usb_state_flag=1;
		//OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_IN);
		//msgQSend(ApQ, MSG_APQ_CONNECT_TO_PC, NULL, NULL, MSG_PRI_NORMAL);
		//type = 1;	//MSG_USBD_PCAM_INIT
		type = 2;	//MSG_USBD_INIT
		msgQSend(ApQ, MSG_APQ_CONNECT_TO_PC, &type, sizeof(INT8U), MSG_PRI_NORMAL);
	}
}

void ap_usb_pcam_isr(void)
{
	if (*P_USBD_INT == 0x8000) {
		*P_USBD_INT |= 0x8000;
		vic_irq_disable(VIC_USB);
		//OSQPost(USBTaskQ, (void *) MSG_USBCAM_PLUG_IN);
		msgQSend(ApQ, MSG_APQ_CONNECT_TO_PC, NULL, NULL, MSG_PRI_NORMAL);
	}
}

void state_usb_entry(void* para1)
{
    INT32U msg_id;
    INT8U err;
    
    USBTaskQ = OSQCreate(usb_task_q_stack, USB_TASK_QUEUE_MAX);
	*P_USBD_CONFIG1 |= 0x100;
/*    *P_USBD_CONFIG &=~0x800;//|= 0x800;	//Switch to USB20PHY
	*P_USBD_CONFIG1 &= ~0x100;//|= 0x100;//[8],SW Suspend For PHY
	OSTimeDly(1);
	usb_initial();
	vic_irq_register(VIC_USB, ap_usb_isr);
	vic_irq_enable(VIC_USB);*/
//	R_SYSTEM_CLK_EN1 = 0x4C98;
//	_devicemount(MINI_DVR_STORAGE_TYPE);
	while (1) {
		msg_id = (INT32U) OSQPend(USBTaskQ, 0, &err);
		if((!msg_id) || (err != OS_NO_ERR)) {
	       	continue;
	    }
		//usb_uninitial();
		//s_usbd_pin = 1;
	    switch (msg_id) {
	    
	        case MSG_USBD_INITIAL:
	           *P_USBD_CONFIG1 &= ~0x100;//|= 0x100;//[8],SW Suspend For PHY

			   	usb_state_flag = 2;
	        	vic_irq_register(VIC_USB, ap_usb_isr);
   				vic_irq_enable(VIC_USB);
   				usb_initial();
	    		break;	
		   /* case MSG_USBD_INIT:
		    case MSG_USBD_PCAM_INIT:
				*P_USBD_CONFIG1 &= ~0x100;//|= 0x100;//[8],SW Suspend For PHY
//				OSTimeDly(1);
               
				//usb_initial();
				if (msg_id == MSG_USBD_INIT) {
					//vic_irq_register(VIC_USB, ap_usb_isr);
					OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_IN);
				} else {
					//vic_irq_register(VIC_USB, ap_usb_pcam_isr);
					OSQPost(USBTaskQ, (void *) MSG_USBCAM_PLUG_IN);
				}
				//vic_irq_enable(VIC_USB);
		    	
		    	break;*/
		    case MSG_USBD_PLUG_IN:
		        OSTimeDly(15);
		    	state_usb_init();
				DBG_PRINT("USBD state Entry\r\n2");
				while (s_usbd_pin) {
					usb_isr();
				   	usb_std_service_udisk(0);
				   	usb_msdc_service(0);
				  #if USBD_TYPE_SW_BY_KEY == 1
				   	msg_id = (INT32U) OSQAccept(USBTaskQ, &err);
				   	if ((err == OS_NO_ERR) && (msg_id == MSG_USBD_SWITCH)) {
				   		OSQPost(USBTaskQ, (void *) MSG_USBCAM_PLUG_IN);
				   		break;
				   	}
				  #endif
				}
				//state_usb_exit();
				usb_uninitial();
				DBG_PRINT("USBD state Exit\r\n");
				if (s_usbd_pin == 0) {
					*P_USBD_CONFIG1 |= 0x100;
				}
				OSTimeDly(20);
//		       	usb_initial();
//		       	vic_irq_enable(VIC_USB);
//		       	R_SYSTEM_CLK_EN1 = 0x4C98;
		        break;
		     case MSG_USBCAM_PLUG_IN:
		        capture_ready=0;
		        OSTimeDly(20);
		     	usb_cam_entrance(0);
				//OSTimeDly(100);
				usb_webcam_start();
		     	DBG_PRINT("P-CAM Entry\r\n");
		     	while (s_usbd_pin) {
					usb_std_service(0);
				  #if USBD_TYPE_SW_BY_KEY == 1
					msg_id = (INT32U) OSQAccept(USBTaskQ, &err);
				   	if ((err == OS_NO_ERR) && (msg_id == MSG_USBD_SWITCH)) {
				   		OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_IN);
				   		SendISOData = 0;
				   		SendAudioData = 0;
				   		break;
				   	}
				  #endif 
				}
				DBG_PRINT("P-CAM Exit\r\n");
				usb_webcam_stop();
				OSTimeDly(30);
   				usb_cam_exit();
   				vic_irq_disable(VIC_USB);
   				usb_uninitial();
   				if (s_usbd_pin == 0) {
   					*P_USBD_CONFIG1 |= 0x100;
   				}
   				OSTimeDly(20);
//   			R_SYSTEM_CLK_EN1 = 0x4C98;
		        break;
	   	}
	}  	
}

#define STG_CFG_CF      0x0001
#define STG_CFG_SD      0x0002
#define STG_CFG_MS      0x0004
#define STG_CFG_USBH    0x0008
#define STG_CFG_USBD    0x0010
#define STG_CFG_NAND1   0x0020
#define STG_CFG_XD      0x0040

void sys_usbd_state_flag_set(INT8U in_or_out)
{
	usbd_flag = in_or_out;
}

INT8U sys_usbd_state_flag_get(void)
{
	return usbd_flag;
}
