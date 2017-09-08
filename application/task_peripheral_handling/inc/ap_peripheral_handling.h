#include "task_peripheral_handling.h"

#define PERI_TIME_INTERVAL_POWER_OFF	1280	//128 = 1s
#define PERI_TIME_INTERVAL_AD_DETECT	2//64		//128 = 1s
#define PERI_TIME_INTERVAL_USBD_DETECT	16		//128 = 1s
#define PERI_TIME_INTERVAL_LED_FLASH	1		//128 = 1s
#define PERI_TIME_INTERVAL_KEY_DETECT	2//1		//128 = 1s
#define Long_Single_width               32
#define Short_Single_width              6
#define USB_DETECT_CNT					5


#define PERI_ADP_OUT_PWR_OFF_TIME       1*128/PERI_TIME_INTERVAL_AD_DETECT //15s	//wwj modify from 15sec to 5sec

#if USE_ADKEY_NO
	#if GPDV_BOARD_VERSION == GPDV_EMU_V2_0
		#define AD_KEY_1_MIN				0xB000
		#define AD_KEY_2_MAX				0xA520
		#define AD_KEY_2_MIN				0x7320
		#define AD_KEY_3_MAX				0x6F10
		#define AD_KEY_3_MIN				0x4380
		#define AD_KEY_4_MAX				0x4E20
		#define AD_KEY_4_MIN				0x2710
	#else
		#define AD_KEY_1_MIN				0xE678
		#define AD_KEY_2_MAX				0xC350
		#define AD_KEY_2_MIN				0x9088
		#define AD_KEY_3_MAX				0x7D00
		#define AD_KEY_3_MIN				0x5DC0
		#define AD_KEY_4_MAX				0x4100
		#define AD_KEY_4_MIN				0x30D4
	#endif
#endif

typedef void (*KEYFUNC)(INT16U *tick_cnt_ptr);

typedef struct
{
	KEYFUNC key_function;
	INT16U key_io;
	INT16U key_cnt;	
	INT16U key_up_cnt;
	INT16U key_press_times;
	INT16U key_active;
	INT16U key_long;
}KEYSTATUS;

extern void ap_peripheral_init(void);
extern void ap_peripheral_key_judge(void);
extern void ap_peripheral_key_register(INT8U type);
extern void ap_peripheral_motion_detect_judge(void);
extern void ap_peripheral_motion_detect_start(void);
extern void ap_peripheral_motion_detect_stop(void);

extern void ap_peripheral_ad_key_judge(void);

extern void ap_peripheral_usbd_detect_init(void);
extern void ap_peripheral_usbd_detect_check(INT8U *debounce_cnt);
extern void ap_peripheral_auto_power_off_set(INT8U type);
extern void ap_peripheral_led_flash_power_off_set(INT32U mode);
extern void LED_pin_init(void);
extern void set_led_mode(LED_MODE_ENUM mode);
extern void LED_blanking_isr(void);
extern void led_red_on(void);
extern void led_green_on(void);
extern void led_red_off(void);
extern void led_green_off(void);
extern void led_all_off(void);
extern void ap_peripheral_charge_det(void);
extern void ap_peripheral_adaptor_out_judge(void);

extern void ap_peripheral_battery_check_calculate(void);
