#include "task_state_handling.h"

extern void ap_state_handling_connect_to_pc(INT8U type);
extern void ap_state_handling_disconnect_to_pc(void);
extern void ap_state_handling_storage_id_set(INT8U stg_id);
extern INT8U ap_state_handling_storage_id_get(void);

extern void ap_state_handling_led_off(void);

extern void ap_state_handling_power_off_handle(INT32U msg);
extern void ap_state_handling_auto_power_off_handle(void);
extern void ap_state_handling_auto_power_off_set(INT8U type);
