
#include "task_peripheral_handling.h"

MSG_Q_ID		PeripheralTaskQ;
void *			peripheral_task_q_stack[PERIPHERAL_TASK_QUEUE_MAX];
static INT8U	peripheral_para[PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN];
extern INT8U usb_state_get(void);
extern INT8S* uart_recive_data(INT8S* buf, INT32U len);

INT8U usb_charge_cnt = 0;

//	prototypes
void task_peripheral_handling_init(void);


void task_peripheral_handling_init(void)
{
	PeripheralTaskQ 	= msgQCreate(PERIPHERAL_TASK_QUEUE_MAX, PERIPHERAL_TASK_QUEUE_MAX,
		 PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN);
	ap_peripheral_init();
	msgQSend(ApQ, MSG_APQ_PERIPHERAL_TASK_READY, NULL, NULL, MSG_PRI_NORMAL);
}


//////////////////////////////////////////////////////////////////////////
INT32S uart_recive_data_set_calendar(INT8S *buf)
{
	INT16U wtemp;
	INT32S nRet, i;
	INT8U  data;
	TIME_T	time_set, time_get;
	INT8S *pbuf = buf;
	
	if(!buf)
	{
		__msg("recive none message\n");
		return -1;
	}
	__msg("buf: %s\n", buf);
	if(pbuf[0] != 0x55)
	{
		__msg("wrong message\n");
		return -1;
	}
	//year
	wtemp = 0;
	data = pbuf[1] ;//- 0x30;
	wtemp += data*100;

	data = pbuf[2] ;//- 0x30;
	wtemp += data*1;

//	data = pbuf[3] ;//- 0x30;
//	wtemp += data*10;

//	data = pbuf[4] ;//- 0x30;
//	wtemp += data*1;
	
	if((wtemp > 2025) || (wtemp < 2015))
	{
		__msg("illege data\n");
		__msg("wtemp = %d\n", wtemp);
		return -1;
	}
	time_set.tm_year = wtemp;
	
	//month
	wtemp = 0;
	data = pbuf[3] ;//- 0x30;
	wtemp += data*1;

//	data = pbuf[6] ;//- 0x30;
//	wtemp += data*1;
	
	if(wtemp>12)
	{
		__msg("illege month\n");
		__msg("wtemp = %d\n", wtemp);
		return -1;
	}
	time_set.tm_mon = wtemp;
	
	//day
	wtemp = 0;
	data = pbuf[4] ;//- 0x30;
	wtemp += data*1;
	
//	data = pbuf[8] ;//- 0x30;
//	wtemp += data*1;
	
	if(wtemp>31)
	{
		__msg("illege day\n");
		return -1;
	}
	time_set.tm_mday = wtemp;
	
	//hour
	wtemp = 0;
	data = pbuf[5] ;//- 0x30;
	wtemp += data*1;

//	data = pbuf[10] ;//- 0x30;
//	wtemp += data*1;
	
	if(wtemp>23)
	{
		__msg("illege hour\n");
		return -1;
	}
	time_set.tm_hour = wtemp;

	//minute
	wtemp = 0;
	data = pbuf[6] ;//- 0x30;
	wtemp += data*1;

//	data = pbuf[12] ;//- 0x30;
//	wtemp += data*1;
	
	if(wtemp>59)
	{
		__msg("illege minute\n");
		return -1;
	}
	time_set.tm_min = wtemp;

	//second
	wtemp = 0;
	data = pbuf[7] ;//- 0x30;
	wtemp += data*1;

//	data = pbuf[14] ;//- 0x30;
//	wtemp += data*1;
	
	if(wtemp>59)
	{
		__msg("illege second\n");
		return -1;
	}
	time_set.tm_sec = wtemp;

//	for(i = 1; i<10; i++)
//	{
//		nRet += pbuf[i]; //- 0x30;
//		if((INT8U)nRet != pbuf[15])
//		{
//			__msg("modify error\n");
//			return -1;
//		}
//	}
	if(pbuf[9] != 0xee)
	{
		__msg("data recive error\n");
		return -1;

	}
	
	cal_time_set(time_set);
	cal_time_get(&time_get);
	__msg("%d/%d/%d %d:%d:%d\n", time_get.tm_year, time_get.tm_mon, time_get.tm_mday, time_get.tm_hour,\
		time_get.tm_min, time_get.tm_sec);
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////

INT8U			card_space_less_flag = 0;


void task_peripheral_handling_entry(void * para)
{
	INT32U			msg_id;
	INT8S 			recive_buf[10];
	INT32U			type;
	INT8U			usbd_debounce_cnt;
	INT8U usb_detect_start;
	INT32U			bat_ck_timerid;
	static INT32U	send_cnt = 0;
	
	//	INT8U  bat_check;
	usb_detect_start	= 0;
	bat_ck_timerid		= 0xFF;
			
	//	bat_check=0;
	usbd_debounce_cnt	= 0;
	task_peripheral_handling_init();

	while (1)
	{
		if (msgQReceive(PeripheralTaskQ, &msg_id, peripheral_para, PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN) == STATUS_FAIL)
		{
			continue;
		}

		switch (msg_id)
		{
			case MSG_PERIPHERAL_TASK_KEY_DETECT:
				ap_peripheral_key_judge();
				break;

			case MSG_PERIPHERAL_TASK_KEY_REGISTER:
				ap_peripheral_key_register(peripheral_para[0]);
				break;

			case MSG_PERIPHERAL_TASK_USBD_DETECT_INIT:
				usb_detect_start = 1;
				ap_peripheral_usbd_detect_init();
				break;

			case MSG_PERIPHERAL_TASK_USBD_DETECT_CHECK:
        		ap_peripheral_usbd_detect_check(&usbd_debounce_cnt);
				break;

			case MSG_PERIPHERAL_TASK_LED_SET:
				set_led_mode(peripheral_para[0]);

				if (peripheral_para[0] == LED_CARD_NO_SPACE)
					card_space_less_flag = 1;

				break;

			case MSG_PERIPHERAL_TASK_LED_FLASH_SET:
				ap_peripheral_led_flash_power_off_set(peripheral_para[0]);
				break;

			case MSG_PERIPHERAL_TASK_LED_FLASH:
				break;

			case MSG_PERIPHERAL_TASK_BAT_CHECK:
				//if (bat_ck_timerid == 0xFF) {
				//	bat_ck_timerid = BATTERY_DETECT_TIMER_ID;
				//	sys_set_timer((void*)msgQSend, (void*)PeripheralTaskQ, MSG_PERIPHERAL_TASK_BAT_STS_FULL, bat_ck_timerid, PERI_TIME_INTERVAL_BAT_CHECK);
				//}
				break;

			case MSG_PERIPHERAL_TASK_BAT_STS_FULL:
				//sys_kill_timer(bat_ck_timerid);
				//bat_ck_timerid = 0xFF;
				type = LED_WAITING_RECORD;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
				break;

#if USE_ADKEY_NO
			case MSG_PERIPHERAL_TASK_AD_DETECT_CHECK:
				ap_peripheral_key_judge();
				
				ap_peripheral_ad_key_judge();

				if ((s_usbd_pin)||(usb_state_get()))
				{
					if (++usb_charge_cnt > 31)
					{
						ap_peripheral_charge_det();
						usb_charge_cnt		= 0;
					}
				}
				if((++send_cnt > 63))
				{
					//uart_send_data(send_cnt--);
					send_cnt = 0;
					

					if(uart_recive_data(recive_buf, 10))
					{
						uart_recive_data_set_calendar(recive_buf);
					}
				}
				
				if (usb_detect_start == 1)
				{
					ap_peripheral_adaptor_out_judge();
				}
				
        		//ap_peripheral_usbd_detect_check(&usbd_debounce_cnt);
				break;

#endif

#if C_BATTERY_DETECT						== CUSTOM_ON && USE_ADKEY_NO
			case MSG_PERIPHERAL_TASK_BAT_STS_REQ:
				//ap_peripheral_battery_sts_send();
				break;

#endif

#if C_MOTION_DETECTION						== CUSTOM_ON
			case MSG_PERIPHERAL_TASK_MOTION_DETECT_JUDGE:
				ap_peripheral_motion_detect_judge();
				break;

			case MSG_PERIPHERAL_TASK_MOTION_DETECT_START:
				ap_peripheral_motion_detect_start();
				break;

			case MSG_PERIPHERAL_TASK_MOTION_DETECT_STOP:
				ap_peripheral_motion_detect_stop();
				break;

#endif

			case MSG_PERIPHERAL_TASK_AUTO_POWER_OFF_SET:
				ap_peripheral_auto_power_off_set(peripheral_para[0]);
				break;

			default:
				break;
		}

	}
}

