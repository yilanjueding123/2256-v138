#ifndef __drv_l1_SYSTEM_H__
#define __drv_l1_SYSTEM_H__

#include "driver_l1.h"
#include "drv_l1_sfr.h"

#define   GPIO_CLK_SW  		       		0x00000004
#define   TIMERS_CLK_SW        			0x00000010
#define   GTE_CLK_SW           		 	0x00000020
#define   DAC_CLK_SW           		    0x00000040
#define   UART_EFUSE_CLK_SW    			0x00000080
#define   MP3_CLK_SW           			0x00000100
#define   SPI0_SPI1_CLK_SW     		 	0x00000200
#define   ADC_CLK_SW       		   		0x00000400
#define   PPU_LINEBASE_CLK_SW	   		0x00000800
#define   SPU_CLK_SW       		        0x00001000
#define   TFT_CLK_SW          		 	0x00002000
#define   SDC_CLK_SW       		     	0x00008000

#define   MS_CLK_SW        		   		0x00000001
#define   UHOST_CLK_SW     		        0x00000002
#define   UDEVICE_CLK_SW   	  		    0x00000004
#define   STN_CLK_SW   	    		    0x00000008
#define   TV_CLK_SW            			0x00000020
#define   PPU_FRAMEBASE_CLK_SW 			0x00000040
#define   CFC_CLK_SW               		0x00000100
#define   KEYSCAN_TOUCH_SENSOR_CLK_SW  	0x00000200
#define   JPEG_CLK_SW  		            0x00000400
#define   DEFLICKER_CLK_SW 				0x00000800
#define   SCALER_CLK_SW		            0x00001000
#define   NAND_CLK_SW		            0x00002000


extern INT32U  MCLK;
extern INT32U  MHZ;

extern void system_enter_halt_mode(void);
extern void system_power_on_ctrl(void);
extern void system_rtc_reset_time_set(void);
extern INT32U system_rtc_counter_addr_get(void);
extern void system_rtc_counter_addr_set(INT32U rtc_cnt);
extern void sys_weak_6M_set(BOOLEAN status);
extern void sys_reg_sleep_mode_set(BOOLEAN status);
extern void sys_ir_power_handler(void);
extern void sys_ir_power_set_pwrkey(INT8U code);
extern void sys_ir_opcode_clear(void);
extern INT32U sys_ir_opcode_get(void);

#endif		// __drv_l1_SYSTEM_H__