#include "ap_state_handling.h"

static INT8U sh_curr_storage_id;
extern INT8U s_usbd_plug_in;


void ap_state_handling_auto_power_off_set(INT8U type)
{		
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_AUTO_POWER_OFF_SET, &type, sizeof(INT8U), MSG_PRI_NORMAL);
}

void ap_state_handling_auto_power_off_handle(void)
{	
	INT32U led_type;
    DBG_PRINT("[auto_power]\r\n");		
	led_type = LED_WAITING_CAPTURE;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_FLASH_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
}

void ap_state_handling_led_off(void)
{
	INT8U			type = DATA_HIGH;

	//msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT8U), MSG_PRI_NORMAL);
}

void ap_state_handling_power_off_handle(INT32U msg)
{
	OS_CPU_SR cpu_sr;
	//INT8U i;
	
	if (s_usbd_pin||s_usbd_plug_in) {
		return;
	}
	OS_ENTER_CRITICAL();
	
	/*if (ap_state_handling_storage_id_get() == NO_STORAGE) {
		for (i=0; i<5; i++) {
			gpio_init_io(LED1, GPIO_OUTPUT);
			gpio_write_io(LED1, DATA_LOW);
			drv_msec_wait(150);
			gpio_init_io(LED1, GPIO_INPUT);
			gpio_set_port_attribute(LED1, ATTRIBUTE_HIGH);
			drv_msec_wait(150);		
		}
	}*/
	
	gpio_write_io(LED1, DATA_LOW);
	gpio_write_io(LED2, DATA_LOW);
	DBG_PRINT("[POWER OFF]\r\n");

	vic_global_mask_set();
    (*((volatile INT32U *) 0xD000000C))|=0x3;
	(*((volatile INT32U *) 0xD0000020)) = 0x0002;		// Disable LVR    
    (*((volatile INT32U *) 0xC0020000))=0;
    (*((volatile INT32U *) 0xC0020000))=(*((volatile INT32U *) 0xC0020000));
    (*((volatile INT32U *) 0xC0020060))=0;
    (*((volatile INT32U *) 0xC0020060))=(*((volatile INT32U *) 0xC0020060));    
    (*((volatile INT32U *) 0xC0030000))=0;
    (*((volatile INT32U *) 0xC0030000))=(*((volatile INT32U *) 0xC0030000));
    (*((volatile INT32U *) 0xC0030008))=0;
    (*((volatile INT32U *) 0xC0030008))=(*((volatile INT32U *) 0xC0030008));    
    (*((volatile INT32U *) 0xC0040050))=0;
    (*((volatile INT32U *) 0xC0040058))=0;    
    (*((volatile INT32U *) 0xd0100020))=0;
    (*((volatile INT32U *) 0xc0060008))=0;
    R_MEM_IO_CTRL &= ~0x7000;
    R_ADC_SETUP = 0;
    R_MIC_SETUP = 0;
    R_ADC_USELINEIN = 0;
    gpio_init_io(PW_EN, GPIO_OUTPUT);
  	gpio_set_port_attribute(PW_EN, ATTRIBUTE_HIGH);
  	R_SYSTEM_CTRL |= 0x3;
  	gpio_write_io(PW_EN, DATA_LOW);
//  	R_SYSTEM_27M &= ~1;
	sys_ldo28_ctrl(0, LDO_28_31V);
  	sys_ldo33_ctrl(0, LDO_33_33V);
	while (1) {
	    volatile INT16U i;
	    
		R_SYSTEM_HALT = 0x500A;
		i = R_CACHE_CTRL;
		
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
		ASM(NOP);
	    
	}
}


void ap_state_handling_connect_to_pc(INT8U type)
{
	INT8U sta;

	if(ap_state_handling_storage_id_get()!= NO_STORAGE) sta = 0; //¨®D?¡§
	else sta = 1;

	//if (type == 1) {
	if (sta == 1) {
		OSQPost(USBTaskQ, (void *) MSG_USBCAM_PLUG_IN);
	} else {
		OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_IN);
	}
/*	INT8U type = USBD_DETECT;
	
	OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_IN);
	msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_STOP, NULL, NULL, MSG_PRI_NORMAL);
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U), MSG_PRI_NORMAL);
	ap_state_handling_icon_show_cmd(ICON_USB_CONNECT, NULL, NULL);*/
}

//extern INT8U s_usbd_pin;	// "extern" isn't good...   Neal
void ap_state_handling_disconnect_to_pc(void)
{
/*	INT8U type = GENERAL_KEY;
	
	//OSQPost(USBTaskQ, (void *) MSG_USBD_PLUG_OUT);
	s_usbd_pin = 0;
	msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U), MSG_PRI_NORMAL);
	ap_state_handling_icon_clear_cmd(ICON_USB_CONNECT, NULL, NULL);*/
	ap_state_handling_power_off_handle(NULL);
}

void ap_state_handling_storage_id_set(INT8U stg_id)
{
	sh_curr_storage_id = stg_id;
}

INT8U ap_state_handling_storage_id_get(void)
{
	return sh_curr_storage_id;
}
