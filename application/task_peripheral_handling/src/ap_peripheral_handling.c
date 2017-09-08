
#include "ap_peripheral_handling.h"
#define LED_ON					1
#define LED_OFF 				0

#if C_MOTION_DETECTION			== CUSTOM_ON
static INT32U	md_work_memory_addr;
static INT32S	motion_detect_cnt;

#endif

#if C_BATTERY_DETECT			== CUSTOM_ON && USE_ADKEY_NO
static INT8U	battery_lvl;
//static INT8U	bat_ck_cnt;

static INT16U	low_voltage_cnt;
static INT32U	battery_value_sum = 0;
static INT8U	bat_ck_cnt = 0;
//extern INT8S	video_record_sts;
INT8U			PREV_LED_TYPE = 0;

#endif

static INT8U	led_flash_timerid;
static INT8U    power_keyup=0;
extern INT8U usb_state_get(void);
extern void usb_state_set(INT8U flag);

#if USE_ADKEY_NO

//static INT8U	ad_line_select = 0;
static INT8U ad_detect_timerid;
//static INT8U ad_18_value_cnt;
//static INT16U ad_value;
//static INT16U ad_18_value;
static INT16U	ad_value;

static KEYSTATUS ad_key_map[USE_ADKEY_NO];

//static INT16U ad_key_cnt = 0;
//static INT8U	bat_ck_timerid ;
//static INT8U	ad_value_cnt ;
#endif
extern INT8S	video_record_sts;
static INT16U	adp_cnt;
INT8U			adp_status;
static INT8U	battery_low_flag = 0;
static INT16U	adp_out_cnt;


static INT32U	key_active_cnt;
static INT8U	power_off_timerid;
static INT8U	usbd_detect_io_timerid;
static INT32U	led_mode;
static KEYSTATUS key_map[USE_IOKEY_NO];

static INT8U	key_detect_timerid;
static INT8U	g_led_count;
static INT8U	g_led_r_state; //0 = OFF;	1=ON;	2=Flicker
static INT8U	g_led_g_state;
static INT8U	g_led_flicker_state; //0=同时闪烁	1=交替闪烁

//	prototypes
void ap_peripheral_key_init(void);
void ap_peripheral_rec_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_capture_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_function_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_next_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_prev_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_ok_key_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_usbd_plug_out_exe(INT16U * tick_cnt_ptr);
void ap_peripheral_video_key_exe(INT16U * tick_cnt_ptr);

void ap_peripheral_null_key_exe(INT16U * tick_cnt_ptr);

#if USE_ADKEY_NO
void ap_peripheral_ad_detect_init(INT8U adc_channel, void(*bat_detect_isr) (INT16U data));
void ap_peripheral_ad_check_isr(INT16U value);

#endif

static INT8U	up_firmware_flag = 0;
static INT8U	flash_flag = 0;
static INT8U	led_red_flag;
static INT8U	led_green_flag;
extern INT8U ap_state_handling_storage_id_get(void);
extern volatile INT8U pic_down_flag;
extern volatile INT8U video_down_flag;
INT8U			s_usbd_plug_in = 0;
static INT8U	usb_led_flag = 0;
static INT8U	tf_card_state_led = 0;


static void init_usbstate(void)
{

	static INT8U	usb_dete_cnt = 0;
	static INT8U	err_cnt = 0;

	while (++err_cnt < 100)
	{
		if (sys_pwr_key1_read())
			usb_dete_cnt++;
		else 
		{
			usb_dete_cnt		= 0;
			break;
		}

		if (usb_dete_cnt > 3)
			break;

		OSTimeDly(2);
	}

	if (usb_dete_cnt > 3)
		usb_state_set(3);

	err_cnt 			= 0;


}


void ap_peripheral_init(void)
{
	power_off_timerid	= usbd_detect_io_timerid = led_flash_timerid = 0xFF;
	key_detect_timerid	= 0xFF;

	init_usbstate();
	
	ap_peripheral_key_init();
	LED_pin_init();

	gpio_init_io(C_USBDEVICE_PIN, GPIO_INPUT);
	gpio_set_port_attribute(C_USBDEVICE_PIN, ATTRIBUTE_HIGH);
	gpio_write_io(C_USBDEVICE_PIN, DATA_LOW);

	
	gpio_init_io(CHARGE_PIN, GPIO_INPUT);
	gpio_set_port_attribute(CHARGE_PIN, ATTRIBUTE_LOW);
	gpio_write_io(CHARGE_PIN, 1);					//pull low
	
#if USE_ADKEY_NO
	ad_detect_timerid = 0xFF;
	ap_peripheral_ad_detect_init(AD_KEY_DETECT_PIN, ap_peripheral_ad_check_isr);
#else
	adc_init();	
#endif
}


INT8U read_usb_pin(void)
{
	INT8U			cnt = 0;
	INT8U			t;

	for (t = 0; t < 5; t++)
	{
		if (gpio_read_io(C_USBDEVICE_PIN))
			cnt++;
		else 
			cnt = 0;
	}

	if (cnt >= 5)
		return 1;

	else 
		return 0;
}


void LED_pin_init(void)
{
	INT32U			type;

	//led init as ouput pull-low
	gpio_init_io(LED1, GPIO_OUTPUT);
	gpio_set_port_attribute(LED1, ATTRIBUTE_HIGH);
	gpio_write_io(LED1, DATA_LOW);
	
	gpio_init_io(LED2, GPIO_OUTPUT);
	gpio_set_port_attribute(LED2, ATTRIBUTE_HIGH);
	gpio_write_io(LED2, DATA_LOW);
	
	led_red_flag		= LED_OFF;
	led_green_flag		= LED_OFF;
	type				= LED_INIT;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
	sys_registe_timer_isr(LED_blanking_isr);		//timer base c to start adc convert
}


extern INT8U	card_space_less_flag;
extern volatile INT8U Secrecy_Failure_Flag;

static void charge_flash_pro(void)
{
	if (gpio_read_io(CHARGE_PIN) == 0)
	{
		led_green_off();
		g_led_r_state		= 2;

	}
	else 
	{
		led_green_off();
		led_red_on();
	}
}


void set_led_mode(LED_MODE_ENUM mode)
{
	INT8U			i;
	static INT8U	prev_mode = 0xaa;
	
	g_led_g_state		= 0;
	g_led_r_state		= 0;
	g_led_flicker_state = 0;
	led_mode			= mode;

	switch ((INT32U)
	mode)
	{
		case LED_INIT:
			led_red_on();
			led_green_off();
			DBG_PRINT("led_type = LED_INIT\r\n");
			break;

		case LED_UPDATE_PROGRAM:
			led_red_off();
			g_led_g_state = 1;
			DBG_PRINT("led_type = LED_UPDATE_PROGRAM\r\n");
			break;

		case LED_UPDATE_FAIL:
			led_green_off();
			break;

		case LED_UPDATE_FINISH:
			led_red_off();
			led_green_on();
			DBG_PRINT("led_type = LED_UPDATE_FINISH\r\n");
			break;

		case LED_RECORD:
			led_green_off();

			//led_red_on();
			g_led_r_state = 2;
			g_led_flicker_state = 0;
			DBG_PRINT("led_type = LED_RECORD\r\n");
			break;

		case LED_SDC_FULL:
			sys_release_timer_isr(LED_blanking_isr);
			led_green_on();
			led_red_off();
			OSTimeDly(15);
			DBG_PRINT("led_type = LED_SDC_FULL\r\n");
			sys_registe_timer_isr(LED_blanking_isr);
		case LED_WAITING_RECORD:
			led_red_off();
			led_green_on();
			DBG_PRINT("led_type = LED_WAITING_RECORD\r\n");
			break;

		case LED_AUDIO_RECORD:
			DBG_PRINT("led_type = LED_AUDIO_RECORD\r\n");
			break;

		case LED_WAITING_AUDIO_RECORD:
			DBG_PRINT("led_type = LED_WAITING_AUDIO_RECORD\r\n");
			break;

		case LED_CAPTURE:
			led_green_off();
			led_red_on();
			DBG_PRINT("led_type = LED_CAPTURE\r\n");
			break;

		case LED_CARD_DETE_SUC:
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;

			sys_release_timer_isr(LED_blanking_isr);

			for (i = 0; i < 3; i++)
			{
				led_red_off();
				led_green_on();
				OSTimeDly(20);
				led_red_on();
				led_green_off();
				OSTimeDly(20);
			}

			led_red_off();
			led_green_on();
			sys_registe_timer_isr(LED_blanking_isr);
			DBG_PRINT("led_type = LED_CARD_DETE_SUC\r\n");
			break;

		case LED_CAPTURE_FAIL:
		case LED_WAITING_CAPTURE:
			led_red_off();
			led_green_on();
			DBG_PRINT("led_type = LED_WAITING_CAPTURE\r\n");
			break;

		case LED_MOTION_DETECTION:
			DBG_PRINT("led_type = LED_MOTION_DETECTION\r\n");
			break;

		case LED_NO_SDC:
			led_green_off();
			led_red_off();

			//g_led_r_state=2;
			DBG_PRINT("led_type = LED_NO_SDC\r\n");
			break;

		case LED_TELL_CARD:
			// sys_release_timer_isr(LED_blanking_isr);
			// led_red_on();
			// OSTimeDly(15);
			//led_red_off();
			//DBG_PRINT("led_type = LED_TELL_CARD\r\n");
			// sys_registe_timer_isr(LED_blanking_isr);
			break;

		case LED_CARD_NO_SPACE:
			//led_all_off();
			// if(storage_sd_upgrade_file_flag_get() == 2)
			// break;
			// g_led_g_state = 3;	//3ìDò?òê±???üD?íê±?￡?á???μ?í?ê±éá??
			// g_led_r_state = 3;
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;

			led_green_off();
			led_red_off();
			DBG_PRINT("led_type = LED_CARD_NO_SPACE\r\n");
			break;

		case LED_CARD_TELL_FULL:
			//led_green_on();
			//g_led_r_state=3;
			DBG_PRINT("led_type = LED_CARD_TELL_FULL\r\n");
			break;

		case LED_USB_CONNECT:
//			if (usb_state_get())
//			{
//				charge_flash_pro();
//			}
//			else 
//			{
				led_green_on();
				led_red_off();
				g_led_g_state = 2;
				usb_led_flag = 1;
				DBG_PRINT("led_type = LED_USB_CONNECT\r\n");
//			}
			break;
		case LED_CHARGE_FULL:
			led_green_off();
			led_red_on();
			DBG_PRINT("led_type = LED_CHARGE_FULL\r\n");
			break;
		case LED_CHARGEING:
			if (prev_mode != mode)
			{
				g_led_count 		= 0;
			}

			led_green_off();
			g_led_r_state = 2;
			DBG_PRINT("led_type = LED_CHARGEING\r\n");
			break;	
		case LED_LOW_BATT:
			if (storage_sd_upgrade_file_flag_get() == 2)
				break;
#if 1
			sys_release_timer_isr(LED_blanking_isr);

			for (i = 0; i < 3; i++)
			{
				led_red_on();
				led_green_on();
				OSTimeDly(25);
				led_green_off();
				led_red_off();
				OSTimeDly(25);
			}

			sys_registe_timer_isr(LED_blanking_isr);
#endif
			msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			break;
	}

	prev_mode = mode;
}


void led_red_on(void)
{
	if (led_red_flag != LED_ON)
	{
		led_red_flag		= LED_ON;
		gpio_write_io(LED2, DATA_HIGH);
	}
}


void led_green_on(void)
{
	if (led_green_flag != LED_ON)
	{
		led_green_flag		= LED_ON;
		gpio_write_io(LED1, DATA_HIGH);
	}
}


void led_red_off(void)
{
	if (led_red_flag != LED_OFF)
	{
		led_red_flag		= LED_OFF;
		gpio_write_io(LED2, DATA_LOW);
	}
}


void led_green_off(void)
{
	if (led_green_flag != LED_OFF)
	{
		led_green_flag		= LED_OFF;
		gpio_write_io(LED1, DATA_LOW);
	}
}


void led_all_off(void)
{
	led_red_off();
	led_green_off();
}


#if SECRECY_ENABLE
static INT8U	SecrecyErr = 1;
static INT32U	SececyTimeCnt = 0;
static INT32U	SececyLedCnt = 0;

#endif


void LED_blanking_isr(void)
{
	if (g_led_count++ == 127)
	{
		g_led_count 		= 0;

		//DBG_PRINT("***\r\n");
	}

	//--------------------------------------------------
#if SECRECY_ENABLE

	if ((Secrecy_Failure_Flag != 0) && (SecrecyErr != 0))
	{ //256*30 约60秒
		led_green_off();

		if (SececyTimeCnt == 0)
			led_red_on();
		else if (SececyTimeCnt == 7)
			led_red_off();

		if (++SececyTimeCnt >= 255)
		{
			SececyTimeCnt		= 0;

			if (++SececyLedCnt > 30)
			{
				SecrecyErr			= 0;
				return;
			}
		}
	}

#endif

	//--------------------------------------------------
	if (g_led_g_state == 1)
	{
		if (g_led_count % 8 == 0)
		{
			if (up_firmware_flag == 1)
			{
				led_green_off();
				up_firmware_flag	= 0;
			}
			else 
			{
				led_green_on();
				up_firmware_flag	= 1;
			}
		}
	}
	else if (g_led_g_state == 2)
	{
		if (g_led_count / 64 == g_led_flicker_state)
			led_green_on();
		else 
			led_green_off();
	}
	else if (g_led_g_state == 3)
	{
		if (g_led_count % 32 == 0)
		{
			if (flash_flag == 0)
			{
				led_green_on();
				flash_flag			= 1;
			}
			else 
			{
				led_green_off();
				flash_flag			= 0;
			}
		}
	}

	if (g_led_r_state == 2)
	{
		if (g_led_count < 64)
			led_red_on();
		else 
			led_red_off();
	}
	else if (g_led_r_state == 3)
	{
		if (g_led_count % 32 == 0)
		{
			if (flash_flag == 0)
			{
				flash_flag			= 1;
				led_red_on();
			}
			else 
			{
				flash_flag			= 0;
				led_red_off();
			}
		}
	}
	else if (g_led_r_state == 4)
	{
		if (g_led_count % 8 == 0)
		{
			if (flash_flag == 0)
			{
				flash_flag			= 1;
				led_red_on();
			}
			else 
			{
				flash_flag			= 0;
				led_red_off();
			}
		}
	}
}


void ap_peripheral_led_flash_power_off_set(INT32U mode)
{
	if (power_off_timerid == 0xFE)
	{
		return;
	}

	if (power_off_timerid != 0xFF)
	{
		sys_kill_timer(power_off_timerid);
		power_off_timerid	= 0xFF;
	}

	if (!s_usbd_pin)
	{
		set_led_mode(mode);

		if (power_off_timerid == 0xFF)
		{
			power_off_timerid	= POWER_OFF_TIMER_ID;
			sys_set_timer((void *) msgQSend, (void *) ApQ, MSG_APQ_POWER_KEY_ACTIVE, power_off_timerid,
				 (PERI_TIME_INTERVAL_POWER_OFF >> 1));
			power_off_timerid	= 0xFE;
		}
	}
}


void ap_peripheral_usbd_detect_init(void)
{
	if (usbd_detect_io_timerid == 0xFF)
	{
		usbd_detect_io_timerid = USBD_DETECT_IO_TIMER_ID;
		sys_set_timer((void *) msgQSend, (void *) PeripheralTaskQ, MSG_PERIPHERAL_TASK_USBD_DETECT_CHECK,
			 usbd_detect_io_timerid, PERI_TIME_INTERVAL_USBD_DETECT);
	}
}


void ap_peripheral_usbd_detect_check(INT8U * debounce_cnt)
{
	INT8U			io_temp;						// type;

	io_temp 			= gpio_read_io(C_USBDEVICE_PIN);

	if (io_temp ^ s_usbd_plug_in)
	{
		if (++ *debounce_cnt > USB_DETECT_CNT)
		{
			*debounce_cnt		= 0;

			if (s_usbd_plug_in == 0)
			{
				if (led_flash_timerid != 0xFF)
				{
					sys_kill_timer(led_flash_timerid);
					led_flash_timerid	= 0xFF;
				}

				s_usbd_plug_in		= 1;

				// ap_peripheral_key_register(USBD_DETECT);
				//	if (ad_value > 30000) {
				//		type = 1;	//MSG_USBD_PCAM_INIT
				//	} else {
				//		type = 2;	//MSG_USBD_INIT
				//	}
				//type = 2;
				//msgQSend(ApQ, MSG_APQ_CONNECT_TO_PC, &type, sizeof(INT8U), MSG_PRI_NORMAL);
				//msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_STOP, NULL, NULL, MSG_PRI_NORMAL);
				//msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_USB_IN, NULL, NULL, MSG_PRI_NORMAL);
				OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);

				//set_led_mode(LED_RECORD);
			}
			else if (s_usbd_plug_in == 1)
			{
				s_usbd_plug_in		= 0;
				s_usbd_pin			= 0;
				msgQSend(ApQ, MSG_APQ_DISCONNECT_TO_PC, NULL, NULL, MSG_PRI_NORMAL);
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
				ap_peripheral_key_register(GENERAL_KEY);
			}
		}

		if (s_usbd_pin)
		{
#if USBD_TYPE_SW_BY_KEY 					== 0
			ap_peripheral_key_register(USBD_DETECT);

#else

			ap_peripheral_key_register(USBD_DETECT2);
#endif

			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_STOP, NULL, NULL, MSG_PRI_NORMAL);
			msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_USB_IN, NULL, NULL, MSG_PRI_NORMAL);
		}
	}
	else 
	{
		*debounce_cnt		= 0;
	}

#if 0

	if (s_usbd_pin || s_usbd_plug_in)
	{

		//	DBG_PRINT("ad_value= %d\r\n",ad_value);
		if (ad_value > 42000 && ad_value < 45000)
		{

			if (ad_value_cnt != 0xFF)
			{

				ad_value_cnt++;

				if (ad_value_cnt > 10)
				{
					//	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_BAT_CHECK, NULL, NULL, MSG_PRI_NORMAL);	
					msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_BAT_STS_FULL, NULL, NULL, MSG_PRI_NORMAL);
					ad_value_cnt		= 0xFF;
				}
			}
		}
		else 
		{
			ad_value_cnt		= 0;
		}
	}

#endif
}


#if USE_ADKEY_NO


void ap_peripheral_ad_detect_init(INT8U adc_channel, void(*ad_detect_isr) (INT16U data))
{
#if C_BATTERY_DETECT				== CUSTOM_ON
	battery_value_sum	= 0;
	bat_ck_cnt			= 0;
#endif

	//	ad_value_cnt = 0;
	adc_init();
	adc_vref_enable_set(TRUE);
	adc_conv_time_sel(4);
	adc_manual_ch_set(adc_channel);
	adc_manual_callback_set(ad_detect_isr);

	if (ad_detect_timerid == 0xFF)
	{
		ad_detect_timerid	= AD_DETECT_TIMER_ID;
		sys_set_timer((void *) msgQSend, (void *) PeripheralTaskQ, MSG_PERIPHERAL_TASK_AD_DETECT_CHECK,
			 ad_detect_timerid, PERI_TIME_INTERVAL_AD_DETECT);
	}	
}


void ap_peripheral_ad_check_isr(INT16U value)
{
	ad_value			= value;
}


INT32U			b1, b2, b3;

enum 
{
	BATTERY_CNT = 64, 
//	BATTERY_Lv3 = 2200 * BATTERY_CNT, 
//	BATTERY_Lv2 = 2175 * BATTERY_CNT, 
//	BATTERY_Lv1 = 2155 * BATTERY_CNT, 
//	BATTERY_Lv0 = 2135 * BATTERY_CNT
	BATTERY_Lv3 = 2338 * BATTERY_CNT, 
	BATTERY_Lv2 = 2318 * BATTERY_CNT, 
	BATTERY_Lv1 = 2298 * BATTERY_CNT, 
	BATTERY_Lv0 = 2278 * BATTERY_CNT
};

static INT32U	ad_time_stamp;

INT32U			previous_direction = 0;
extern void ap_state_handling_led_off(void);
extern INT8U	display_str_battery_low;

#define BATTERY_GAP 			10*BATTERY_CNT


static INT8U ap_peripheral_smith_trigger_battery_level(INT32U direction)
{
	static INT8U	bat_lvl_cal_bak = (INT8U)BATTERY_Lv3;
	INT8U			bat_lvl_cal;

	//__msg("battery_value_sum: %d\n", battery_value_sum);
	if (battery_value_sum >= BATTERY_Lv3)
	{
		bat_lvl_cal 		= 4;
	}
	else if ((battery_value_sum < BATTERY_Lv3) && (battery_value_sum >= BATTERY_Lv2))
	{
		bat_lvl_cal 		= 3;
	}
	else if ((battery_value_sum < BATTERY_Lv2) && (battery_value_sum >= BATTERY_Lv1))
	{
		bat_lvl_cal 		= 2;
	}
	else if ((battery_value_sum < BATTERY_Lv1) && (battery_value_sum >= BATTERY_Lv0))
	{
		bat_lvl_cal 		= 1;
	}
	else if (battery_value_sum < BATTERY_Lv0)
		bat_lvl_cal = 0;

	if ((direction == 0) && (bat_lvl_cal > bat_lvl_cal_bak))
	{
		if (battery_value_sum >= BATTERY_Lv3 + BATTERY_GAP)
		{
			bat_lvl_cal 		= 4;
		}
		else if ((battery_value_sum < BATTERY_Lv3 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv2 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 3;
		}
		else if ((battery_value_sum < BATTERY_Lv2 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv1 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 2;
		}
		else if ((battery_value_sum < BATTERY_Lv1 + BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv0 + BATTERY_GAP))
		{
			bat_lvl_cal 		= 1;
		}
		else if (battery_value_sum < BATTERY_Lv0 + BATTERY_GAP)
			bat_lvl_cal = 0;
	}


	if ((direction == 1) && (bat_lvl_cal < bat_lvl_cal_bak))
	{
		if (battery_value_sum >= BATTERY_Lv3 - BATTERY_GAP)
		{
			bat_lvl_cal 		= 4;
		}
		else if ((battery_value_sum < BATTERY_Lv3 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv2 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 3;
		}
		else if ((battery_value_sum < BATTERY_Lv2 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv1 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 2;
		}
		else if ((battery_value_sum < BATTERY_Lv1 - BATTERY_GAP) && (battery_value_sum >= BATTERY_Lv0 - BATTERY_GAP))
		{
			bat_lvl_cal 		= 1;
		}
		else if (battery_value_sum < BATTERY_Lv0 - BATTERY_GAP)
			bat_lvl_cal = 0;
	}


	bat_lvl_cal_bak 	= bat_lvl_cal;
	return bat_lvl_cal;

}	


void ap_peripheral_battery_check_calculate(void)
{
	INT8U			bat_lvl_cal;
	INT32U			direction = 0, led_type;

	//__msg("adp_status = %d\n", adp_status);
	if (adp_status == 0)
	{
		//unkown state
		return;
	}
	else if (adp_status == 1)
	{
		//adaptor in state
		direction			= 1;					//low voltage to high voltage

		if (previous_direction != direction)
		{
			//msgQSend(ApQ, MSG_APQ_BATTERY_CHARGED_SHOW, NULL, NULL, MSG_PRI_NORMAL);
		}

		previous_direction	= direction;
	}
	else 
	{
		//adaptor out state
		direction			= 0;					//high voltage to low voltage

		if (previous_direction != direction)
		{
			//msgQSend(ApQ, MSG_APQ_BATTERY_CHARGED_CLEAR, NULL, NULL, MSG_PRI_NORMAL);
		}

		previous_direction	= direction;
	}

	battery_value_sum	+= (ad_value >> 4);

	// DBG_PRINT("%d, ",(ad_value>>4));
	//__msg("%d, ",(ad_value>>4));
	bat_ck_cnt++;

	if (bat_ck_cnt >= BATTERY_CNT)
	{
		//__msg("[%d]\r\n", (ad_value >> 4));
		bat_lvl_cal 		= ap_peripheral_smith_trigger_battery_level(direction);
		//__msg("bat_lvl_cal = %d, direction = %d\n", bat_lvl_cal, direction);
		// DBG_PRINT("b:[%d],", bat_lvl_cal);
		if (!battery_low_flag)
		{
			msgQSend(ApQ, MSG_APQ_BATTERY_LVL_SHOW, &bat_lvl_cal, sizeof(INT8U), MSG_PRI_NORMAL);
		}

		if ((bat_lvl_cal == 0) && (direction == 0))
		{
			low_voltage_cnt++;

			if (low_voltage_cnt > 3)
			{
				low_voltage_cnt 	= 0;
				ap_state_handling_led_off();

#if C_BATTERY_LOW_POWER_OFF 					== CUSTOM_ON
				__msg("battery_low_flag = %d\n", battery_low_flag);
				if (!battery_low_flag)
				{
					battery_low_flag	= 1;
					{
						INT8U			type;

						msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_STOP, NULL, NULL, MSG_PRI_NORMAL);
						type				= FALSE;
						msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK_SWITCH, &type, sizeof(INT8U),
							 MSG_PRI_NORMAL);
						type				= BETTERY_LOW_STATUS_KEY;
						msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U),
							 MSG_PRI_NORMAL);

						led_type			= LED_LOW_BATT;
						msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U),
							 MSG_PRI_NORMAL);

					}
				}

#endif
			}
		}
		else 
		{
			if (battery_low_flag)
			{
				INT8U			type;

				battery_low_flag	= 0;
				msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_TIMER_START, NULL, NULL, MSG_PRI_NORMAL);
				type				= GENERAL_KEY;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_REGISTER, &type, sizeof(INT8U), MSG_PRI_NORMAL);
				led_type			= PREV_LED_TYPE;
				msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			}

			low_voltage_cnt 	= 0;
		}

		bat_ck_cnt			= 0;
		battery_value_sum	= 0;
	}
}	


//#define SA_TIME	50	//seconds, for screen saver time. Temporary use "define" before set in "STATE_SETTING".
void ap_peripheral_ad_key_judge(void)
{

	INT32U			t;

	t					= OSTimeGet();

	if ((t - ad_time_stamp) < 2)
	{
		return;
	}

	ad_time_stamp		= t;

	adc_manual_ch_set(AD_KEY_DETECT_PIN);

	//DBG_PRINT("%d, ",(ad_value>>4));
#if C_BATTERY_DETECT				== CUSTOM_ON
	ap_peripheral_battery_check_calculate();
#endif

	adc_manual_sample_start();

}


#endif

static int ap_peripheral_power_key_read(int pin)
{
	int 			status;


//#if (								KEY_TYPE == KEY_TYPE1)||(KEY_TYPE == KEY_TYPE2)||(KEY_TYPE == KEY_TYPE3)||(KEY_TYPE == KEY_TYPE4)||(KEY_TYPE == KEY_TYPE5)
//	status				= gpio_read_io(pin);

//#else

	switch (pin)
	{
		case PWR_KEY0:
			status = sys_pwr_key0_read();
			break;

		case PWR_KEY1:
			status = sys_pwr_key1_read();
			break;
	}

//#endif

	if (status != 0)
		return 1;

	else 
		return 0;
}



void ap_peripheral_adaptor_out_judge(void)
{
	//DBG_PRINT("NA");
	adp_out_cnt++;

	switch (adp_status)
	{
		case 0: //unkown state
			if (ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				//DBG_PRINT("xx:%d",adp_cnt);
				adp_cnt++;

				if (adp_cnt > 16)
				{
					adp_out_cnt 		= 0;
					adp_cnt 			= 0;
					adp_status			= 1;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);

#if C_BATTERY_DETECT								== CUSTOM_ON && USE_ADKEY_NO

					//battery_lvl = 1;
#endif
				}
			}
			else 
			{
				adp_cnt 			= 0;
			}

			//	DBG_PRINT("yy:%d\r\n",adp_out_cnt);
			if (adp_out_cnt > 24)
			{
				adp_out_cnt 		= 0;
				adp_status			= 3;

#if C_BATTERY_DETECT							== CUSTOM_ON && USE_ADKEY_NO

				//battery_lvl = 2;
				low_voltage_cnt 	= 0;
#endif
			}

			break;

		case 1: //adaptor in state
			if (!ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if (adp_out_cnt > 8)
				{
					adp_status			= 2;

#if C_BATTERY_DETECT								== CUSTOM_ON
					low_voltage_cnt 	= 0;
#endif
				}
			}
			else 
			{
				adp_out_cnt 		= 0;
			}

			break;

		case 2: //adaptor out state
			if (!ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if ((adp_out_cnt > PERI_ADP_OUT_PWR_OFF_TIME))
				{
					//DBG_PRINT("1111111111111111111\r\n");
					//adp_out_cnt=320;
#if 1

					//ap_peripheral_pw_key_exe(&adp_out_cnt);
#endif

					adp_out_cnt 		= 0;
					usb_state_set(0);
					msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
				}

				adp_cnt 			= 0;
			}
			else 
			{
				adp_cnt++;

				if (adp_cnt > 3)
				{
					adp_out_cnt 		= 0;
					adp_status			= 1;
					//usbd_exit			= 0;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);
				}
			}

			break;

		case 3: //adaptor initial out state
			if (ap_peripheral_power_key_read(ADP_OUT_PIN))
			{
				if (adp_out_cnt > 3)
				{
					adp_out_cnt 		= 0;
					adp_status			= 1;
					OSQPost(USBTaskQ, (void *) MSG_USBD_INITIAL);
				}
			}
			else 
			{
				if (usb_state_get())
					usb_state_set(0);
				adp_out_cnt 		= 0;
			}

			break;

		default:
			break;
	}	
}



void ap_peripheral_auto_power_off_set(INT8U type)
{
	if (type)
	{
		key_active_cnt		= 0;
	}
	else 
	{
		key_active_cnt		= 0xFFFFFFFF;
	}
}


static INT8U	DV_OutHigh_sta = 1;


void ap_peripheral_key_init(void)
{
	INT32U			i;

	gp_memset((INT8S *) &key_map, NULL, sizeof(KEYSTATUS));

	//-----------------------------------
#if DV_OUT_HIGH_LEVEL

	if (DV_OutHigh_sta)
	{ //开机DV输出高电平 ,  等待500ms,  输出设置在main.c
		DV_OutHigh_sta		= 0;
		gpio_init_io(VIDEO_KEY, GPIO_OUTPUT);
		gpio_set_port_attribute(VIDEO_KEY, ATTRIBUTE_HIGH);
		gpio_write_io(VIDEO_KEY, DATA_LOW);
		OSTimeDly(10);
		gpio_write_io(VIDEO_KEY, DATA_HIGH);		//开机输出高电平500ms
		OSTimeDly(50);
		gpio_write_io(VIDEO_KEY, DATA_LOW);
		OSTimeDly(20);

		//OSTimeDly(50);
	}

#endif


	//-----------------------------------
	ap_peripheral_key_register(GENERAL_KEY);

	for (i = 0; i < USE_IOKEY_NO; i++)
	{
		if (key_map[i].key_io)
		{
			gpio_init_io(key_map[i].key_io, GPIO_INPUT);
			gpio_set_port_attribute(key_map[i].key_io, ATTRIBUTE_LOW);
			gpio_write_io(key_map[i].key_io, (key_map[i].key_active) ^ 1);

			key_map[i].key_press_times = 0;
			key_map[i].key_up_cnt = 0;
			key_map[i].key_cnt	= 0;


		}
	}

//	if (key_detect_timerid == 0xFF)
//	{
//		key_detect_timerid	= PERIPHERAL_KEY_TIMER_ID;
//		sys_set_timer((void *) msgQSend, (void *) PeripheralTaskQ, MSG_PERIPHERAL_TASK_KEY_DETECT, key_detect_timerid,
//			 PERI_TIME_INTERVAL_KEY_DETECT);
//	}
}


void ap_peripheral_key_register(INT8U type)
{
	INT32U			i;

	if (type == GENERAL_KEY)
	{
		key_map[0].key_io	= VIDEO_KEY;
		key_map[0].key_function = (KEYFUNC)ap_peripheral_video_key_exe;
		key_map[0].key_active = VIDEO_ACTIVE_KEY;
		key_map[0].key_long = 0;

		//key_map[1].key_io = CAPTURE_KEY;
		//key_map[1].key_function = (KEYFUNC) ap_peripheral_capture_key_exe;
		//key_map[1].key_active= CAPTURE_ACTIVE_KEY;
		//key_map[1].key_long =0;
		
		ad_key_map[0].key_io = FUNCTION_KEY;
		ad_key_map[0].key_function = (KEYFUNC)ap_peripheral_null_key_exe;


	}
	else if (type == USBD_DETECT)
	{
		//		key_map[0].key_io = C_USBDEVICE_PIN;
		//		key_map[0].key_function = (KEYFUNC) ap_peripheral_usbd_plug_out_exe;
#if USE_IOKEY_NO

		for (i = 0; i < USE_IOKEY_NO; i++)
		{
			key_map[i].key_io	= NULL;
		}

#endif

#if USE_ADKEY_NO

		for (i = 0; i < USE_ADKEY_NO; i++)
		{
			ad_key_map[i].key_function = ap_peripheral_null_key_exe;
		}

#endif

#if USBD_TYPE_SW_BY_KEY 				== 1
	}
	else if (type == USBD_DETECT2)
	{

#if USE_IOKEY_NO

		for (i = 0; i < USE_IOKEY_NO; i++)
		{
			if ((key_map[i].key_io != CAPTURE_ACTIVE_KEY)

#ifdef PW_KEY
			&& (key_map[i].key_io != PW_KEY)
#endif

)
			{
				key_map[i].key_io	= NULL;
			}
		}

#endif

#if USE_ADKEY_NO

		for (i = 0; i < USE_ADKEY_NO; i++)
		{
			ad_key_map[i].key_function = ap_peripheral_null_key_exe;
		}

#endif

#endif
	}
	else if (type == DISABLE_KEY)
	{
		for (i = 0; i < USE_IOKEY_NO; i++)
		{
			key_map[i].key_io	= NULL;
		}
	}
}


#define SA_TIME 				60	//seconds, for screen saver time. Temporary use "define" before set in "STATE_SETTING".


void ap_peripheral_key_judge(void)
{
	INT32U			i;

	//INT32U cnt_sec;
	INT16U			key_down = 0;
		
	for (i = 0; i < USE_IOKEY_NO; i++)
	{
		if (key_map[i].key_io)
		{

			if (key_map[i].key_active)
				key_down = gpio_read_io(key_map[i].key_io);
			else 
				key_down = !gpio_read_io(key_map[i].key_io);

			if (key_down)
			{
				//DBG_PRINT("key%d=1\r\n",i);
				if (!key_map[i].key_long)
				{
					key_map[i].key_cnt	+= 1;

#if SUPPORT_LONGKEY 								== CUSTOM_ON

					if (key_map[i].key_cnt >= Long_Single_width)
					{
						key_map[i].key_long = 1;
						key_map[i].key_function(& (key_map[i].key_cnt));
					}

#endif
				}
				else 
				{
					key_map[i].key_cnt	= 0;
				}

				if (key_map[i].key_cnt == 65535)
				{
					key_map[i].key_cnt	= 16;
				}
			}
			else 
			{
				if (key_map[i].key_long)
				{
					key_map[i].key_long = 0;
					key_map[i].key_cnt	= 0;
				}

				if (key_map[i].key_cnt >= Short_Single_width) //Short_Single_width
				{
					key_map[i].key_function(& (key_map[i].key_cnt));
				}

				key_map[i].key_cnt	= 0;

			}
		}
	}

}


void ap_peripheral_charge_det(void)
{
	INT16U			pin_state = 0;
	static INT16U	prev_pin_state = 0;
	INT16U			led_type;
	static INT8U	loop_cnt = 0;
	static INT16U	prev_ledtype = 0;

	pin_state			= gpio_read_io(CHARGE_PIN);

	//__msg("pin_state=%d\n",pin_state);
	if (pin_state == prev_pin_state)
	{
		loop_cnt++;
	}
	else 
		loop_cnt = 0;

	if (loop_cnt >= 3)
	{


		loop_cnt			= 3;

		if (pin_state)				//zhibo, 20170619
			led_type = LED_CHARGE_FULL;
		else 
		{
			led_type = LED_CHARGEING;
			//__msg("s_usbd_pin:%d, usb_state_get = %d\n", s_usbd_pin,usb_state_get());
		}

		if (prev_ledtype != led_type)
		{
			prev_ledtype		= led_type;

			if (((video_record_sts & 0x02) == 0))
			{
				if (storage_sd_upgrade_file_flag_get() != 2)
					msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
			}
		}
	}

	prev_pin_state		= pin_state;
}

void ap_peripheral_video_key_exe(INT16U * tick_cnt_ptr)
{
	INT32U			led_type;

#if SECRECY_ENABLE

	if ((Secrecy_Failure_Flag != 0) && (SecrecyErr != 0))
		return;

#endif

	if (ap_state_handling_storage_id_get() == NO_STORAGE)
	{

		led_type			= LED_TELL_CARD;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
	}

	if ((!s_usbd_pin) && (!pic_down_flag) && (!card_space_less_flag) && (!video_down_flag))
	{
		if (ap_state_handling_storage_id_get() != NO_STORAGE)
		{
			if (*tick_cnt_ptr >= Long_Single_width)
			{
				__msg("video\n");
				msgQSend(ApQ, MSG_APQ_VIDEO_RECORD_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
			else 
			{
				DBG_PRINT("caputure\n");
				msgQSend(ApQ, MSG_APQ_CAPTUER_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
			}
		}
	}
	else if (card_space_less_flag)
	{
		led_type			= LED_CARD_TELL_FULL;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);

	}

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_capture_key_exe(INT16U * tick_cnt_ptr)
{
	INT32U			led_type;

#if SECRECY_ENABLE

	if ((Secrecy_Failure_Flag != 0) && (SecrecyErr != 0))
		return;

#endif

	if (ap_state_handling_storage_id_get() == NO_STORAGE)
	{

		led_type			= LED_TELL_CARD;
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
	}

	if (!s_usbd_pin)
	{
#if SUPPORT_LONGKEY 					== CUSTOM_ON

		if (*tick_cnt_ptr >= Long_Single_width)
		{
		}
		else 
#endif

		{
			DBG_PRINT("[CAPTUER_ACTIVE...]\r\n");
			msgQSend(ApQ, MSG_APQ_CAPTUER_ACTIVE, NULL, NULL, MSG_PRI_NORMAL);
		}
	}
	else 
	{

	}

	*tick_cnt_ptr		= 0;

}


//============================================================================================
void ap_peripheral_rec_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_next_key_exe(INT16U * tick_cnt_ptr)
{
	*tick_cnt_ptr		= 0;
}


void ap_peripheral_prev_key_exe(INT16U * tick_cnt_ptr)
{
	*tick_cnt_ptr		= 0;
}


void ap_peripheral_function_key_exe(INT16U * tick_cnt_ptr)
{

	*tick_cnt_ptr		= 0;
}


void ap_peripheral_ok_key_exe(INT16U * tick_cnt_ptr)
{

}


void ap_peripheral_usbd_plug_out_exe(INT16U * tick_cnt_ptr)
{
	msgQSend(ApQ, MSG_APQ_DISCONNECT_TO_PC, NULL, NULL, MSG_PRI_NORMAL);
	*tick_cnt_ptr		= 0;
}


void ap_peripheral_menu_key_exe(INT16U * tick_cnt_ptr)
{
	*tick_cnt_ptr		= 0;
}


void ap_peripheral_null_key_exe(INT16U * tick_cnt_ptr)
{

}


