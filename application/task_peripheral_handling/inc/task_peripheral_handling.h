#include "application.h"

#define PERIPHERAL_TASK_QUEUE_MAX			192
#define PERIPHERAL_TASK_QUEUE_MAX_MSG_LEN	10

#define PERI_TIME_INTERVAL_BAT_CHECK	38400  //128*60*5 s

extern MSG_Q_ID PeripheralTaskQ;
extern void ap_peripheral_init(void);
extern void task_peripheral_handling_entry(void *para);
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
extern void led_all_off(void);
extern void ap_peripheral_charge_det(void);
extern void ap_state_handling_led_off(void);
extern void ap_peripheral_adaptor_out_judge(void);


