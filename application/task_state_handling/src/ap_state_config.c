#include "ap_state_config.h"

INT32U CRC32_tbl[256];
SYSTEM_USER_OPTION Global_User_Optins;
USER_ITEMS user_item;

//	prototypes
void ap_state_config_default_set(void);

void ap_state_config_initial(INT32S status)
{
	//default setting
	Global_User_Optins.item.ifdirty = 0;
	if (status == STATUS_FAIL) { /* check nvdb initial status */
		ap_state_config_default_set();
	}
}

void ap_state_config_default_set(void)
{
	// some default config is 0
	gp_memset((INT8S *)&Global_User_Optins, 0x00, sizeof(SYSTEM_USER_OPTION));
	ap_state_resource_user_option_load(&Global_User_Optins);
	Global_User_Optins.item.ifdirty = 1;
}

void ap_state_config_store(void)
{
	if (Global_User_Optins.item.ifdirty == 1) {
		/* calculate CRC */
		CRC_cal((INT8U*)&Global_User_Optins.item, sizeof(USER_ITEMS),Global_User_Optins.crc);
		DBG_PRINT("store CRC = %02x %02x %02x %02x\r\n",Global_User_Optins.crc[0],Global_User_Optins.crc[1],Global_User_Optins.crc[2],Global_User_Optins.crc[3]);
    	nvmemory_user_sector_store(0, (INT32U *)&Global_User_Optins, 1);
    	gp_memcpy((INT8S*)&user_item, (INT8S*)&Global_User_Optins.item, sizeof(USER_ITEMS));
    	nvmemory_user_sector_load(0, (INT32U *)&Global_User_Optins, 1);
    	if (gp_memcmp((INT8S*)&user_item, (INT8S*)&Global_User_Optins.item, sizeof(USER_ITEMS)) != 0) {
    		DBG_PRINT("verify failed, store again\r\n");
			gp_memcpy((INT8S*)&Global_User_Optins.item,(INT8S*)&user_item, sizeof(USER_ITEMS));
    		CRC_cal((INT8U*)&Global_User_Optins.item, sizeof(USER_ITEMS),Global_User_Optins.crc);
    		nvmemory_user_sector_store(0, (INT32U *)&Global_User_Optins, 1);
		}
		Global_User_Optins.item.ifdirty = 0;
    }
}

INT32S ap_state_config_load(void)
{
	INT8U expect_crc[4];

	nvmemory_user_sector_load(0, (INT32U *)&Global_User_Optins, 1);		//load user setting
	DBG_PRINT("load CRC = %02x %02x %02x %02x\r\n",Global_User_Optins.crc[0],Global_User_Optins.crc[1],Global_User_Optins.crc[2],Global_User_Optins.crc[3]);
	/* calculate expect CRC */
	CRC_cal((INT8U*)&Global_User_Optins.item, sizeof(USER_ITEMS),expect_crc);
	DBG_PRINT("expect CRC = %02x %02x %02x %02x\r\n",expect_crc[0],expect_crc[1],expect_crc[2],expect_crc[3]);
	/* compare CRC and expect CRC */
	if (gp_memcmp((INT8S*)expect_crc, (INT8S*)Global_User_Optins.crc, 4) != 0) {
		DBG_PRINT("crc error\r\n");
		return STATUS_FAIL; /* CRC error */
	}
	return STATUS_OK;
}

void ap_state_config_language_set(INT8U language)
{
	if (language != ap_state_config_language_get()) {
		Global_User_Optins.item.language = language;
		Global_User_Optins.item.ifdirty = 1;
	}
}

INT8U ap_state_config_language_get(void)
{
	return Global_User_Optins.item.language;
}

void ap_state_config_volume_set(INT8U sound_volume)
{
	if (sound_volume != ap_state_config_volume_get()) {
		Global_User_Optins.item.sound_volume = sound_volume;
		Global_User_Optins.item.ifdirty = 1;	
	}
}

INT8U ap_state_config_volume_get(void)
{
	return Global_User_Optins.item.sound_volume;
}

void ap_state_config_motion_detect_set(INT8U motion_detect)
{
	if (motion_detect != ap_state_config_motion_detect_get()) {
		Global_User_Optins.item.music_play_mode = motion_detect;
		Global_User_Optins.item.ifdirty = 1;
	}
}

INT8U ap_state_config_motion_detect_get(void)
{
	return Global_User_Optins.item.music_play_mode;
}

void ap_state_config_record_time_set(INT8U record_time)
{
	if (record_time != ap_state_config_record_time_get()) {
		Global_User_Optins.item.full_screen = record_time;
		Global_User_Optins.item.ifdirty = 1;
	}
}

INT8U ap_state_config_record_time_get(void)
{
	return Global_User_Optins.item.full_screen;
}

void ap_state_config_date_stamp_set(INT8U date_stamp)
{
	if (date_stamp != ap_state_config_date_stamp_get()) {
		Global_User_Optins.item.thumbnail_mode = date_stamp;
		Global_User_Optins.item.ifdirty = 1;
	}
}

INT8U ap_state_config_date_stamp_get(void)
{
	return Global_User_Optins.item.thumbnail_mode;
}

void ap_state_config_factory_date_get(INT8U *date)
{
	*date++ = Global_User_Optins.item.factory_date[0];
	*date++ = Global_User_Optins.item.factory_date[1];
	*date = Global_User_Optins.item.factory_date[2];
}

void ap_state_config_factory_time_get(INT8U *time)
{
	*time++ = Global_User_Optins.item.factory_time[0];
	*time++ = Global_User_Optins.item.factory_time[1];
	*time = Global_User_Optins.item.factory_time[2];
}

void ap_state_config_base_day_set(INT32U day)
{
	if (day != ap_state_config_base_day_get()) {
		Global_User_Optins.item.base_day = day;
		Global_User_Optins.item.ifdirty = 1;
	}

}

INT32U ap_state_config_base_day_get(void)
{
	return Global_User_Optins.item.base_day;
}


void CRC32tbl_init(void)
{
    INT32U i,j;
    INT32U c;

    for (i=0 ; i<256 ; ++i) {
        c = i ;
        for (j=0 ; j<8 ; j++) { 
            c = (c & 1) ? (c >> 1) ^ CRC32_POLY : (c >> 1);
        }
        CRC32_tbl[i] = c;
    }
}

void CRC_cal(INT8U *pucBuf, INT32U len, INT8U *CRC)
{
    INT32U i_CRC, value;
    INT32U i;

    if (!CRC32_tbl[1]) {
        CRC32tbl_init();
    }    
    i_CRC = 0xFFFFFFFF;
    for (i=0 ; i<len ; i++) {
        i_CRC = (i_CRC >> 8) ^ CRC32_tbl[(i_CRC ^ pucBuf[i]) & 0xFF];
    }
    value = ~i_CRC;
    CRC[0] = (value & 0x000000FF) >> 0;
    CRC[1] = (value & 0x0000FF00) >> 8;
    CRC[2] = (value & 0x00FF0000) >> 16;
    CRC[3] = (value & 0xFF000000) >> 24;
}