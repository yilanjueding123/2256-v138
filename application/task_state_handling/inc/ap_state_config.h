#include "task_state_handling.h"

#define CRC32_POLY     0xEDB88320

extern void ap_state_config_initial(INT32S status);
extern void ap_state_config_default_set(void);
extern void ap_state_config_store(void);
extern INT32S ap_state_config_load(void);
extern void ap_state_config_language_set(INT8U language);
extern INT8U ap_state_config_language_get(void);
extern void ap_state_config_volume_set(INT8U sound_volume);
extern INT8U ap_state_config_volume_get(void);
extern void ap_state_config_motion_detect_set(INT8U motion_detect);
extern INT8U ap_state_config_motion_detect_get(void);
extern void ap_state_config_record_time_set(INT8U record_time);
extern INT8U ap_state_config_record_time_get(void);
extern void ap_state_config_date_stamp_set(INT8U date_stamp);
extern INT8U ap_state_config_date_stamp_get(void);
extern void ap_state_config_factory_date_get(INT8U *date);
extern void ap_state_config_factory_time_get(INT8U *time);
extern void ap_state_config_base_day_set(INT32U day);
extern INT32U ap_state_config_base_day_get(void);