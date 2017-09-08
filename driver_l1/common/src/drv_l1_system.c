/*
* Purpose: System option setting
*
* Author: wschung
*
* Date: 2008/5/9
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.00
* History :

     1 .Add system_bus_arbiter_init  2008/4/21 wschung
        setting all masters as 0
*/
#include "drv_l1_system.h"

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#if (defined _DRV_L1_SYSTEM) && (_DRV_L1_SYSTEM == 1)             //
//================================================================//

#define INT_EXTAB             0x04000000
#define INT_RTC               0x80000000

INT32U  MCLK = INIT_MCLK;
INT32U  MHZ  = INIT_MHZ;

/* Prototypes */
void system_set_pll(INT32U PLL_CLK);
void system_bus_arbiter_init(void);
void system_enter_halt_mode(void);
void system_power_on_ctrl(void);
INT32U system_rtc_counter_get(void);
void system_rtc_counter_set(INT32U rtc_cnt);
static tREFI_ENUM refresh_period=tREFI_15o6u;

void system_set_pll(INT32U cpu_clock_mhz)
{
	/*// Init SDRAM
	if ((R_MEM_SDRAM_CTRL0==0x5213) &&
        (R_MEM_SDRAM_CTRL1==0x3000) &&
        (R_MEM_SDRAM_TIMING==0x0F8A) &&
        (R_MEM_SDRAM_CBRCYC==0x0100) &&
        (R_MEM_SDRAM_MISC==0x0006)) {

	  #if SDRAM_SIZE == 0x00200000		// 2MB(1x16Mb)
		R_MEM_SDRAM_CTRL0 = 0x5210;
	  #elif SDRAM_SIZE == 0x00800000	// 8MB(4x16Mb)
		R_MEM_SDRAM_CTRL0 = 0x6711;
	  #elif SDRAM_SIZE == 0x01000000	// 16MB(8x16Mb)
		R_MEM_SDRAM_CTRL0 = 0x5712;
	  #else								// 32MB(16x16Mb)
		R_MEM_SDRAM_CTRL0 = 0x5713;		// 0xD0200040
	  #endif
		R_MEM_SDRAM_CTRL1 = 0x2030;		// 0xD0200044
		R_MEM_SDRAM_TIMING = 0x0F8A;	// 0xD0200048
		R_MEM_SDRAM_CBRCYC = 0x05D9;	// 0xD020004C
		R_MEM_SDRAM_MISC = 0x0006;		// 0xD0200050
		R_MEM_DRV = 0x92AA;				// 0xC0000124
}*/
#if USED_MCU_NAME == MCU_GPDV6655D
	if (cpu_clock_mhz >= 115) { 
		//115MHz SDRAM 
		R_MEM_SDRAM_CTRL0 = 0x8010; 
		R_MEM_SDRAM_CTRL1 = 0x3000;                // 0xD0200044 
		R_MEM_SDRAM_TIMING = 0x0F8A;        // 0xD0200048 
		R_MEM_SDRAM_CBRCYC = 0x0730;        // 0xD020004C                // 0x0730(118MHz), 0x6E2(113MHz) 
		R_MEM_SDRAM_MISC = 0x0006;                // 0xD0200050 
		R_MEM_DRV = 0x5555;                                // 0xC0000124                // 0x0000=4mA, 0x5555=8mA, 0xAAAA=12mA 

	} else if (cpu_clock_mhz >= 108) { 
		//108MHz SDRAM 
		R_MEM_SDRAM_CTRL0 = 0x8010; 
		R_MEM_SDRAM_CTRL1 = 0x3000;                // 0xD0200044 
		R_MEM_SDRAM_TIMING = 0x0F8A;        // 0xD0200048 
		R_MEM_SDRAM_CBRCYC = 0x0694;        // 0xD020004C 
		R_MEM_SDRAM_MISC = 0x0006;                // 0xD0200050 
		R_MEM_DRV = 0x5555;                                // 0xC0000124                // 0x0000=4mA, 0x5555=8mA, 0xAAAA=12mA 

	} else { 
		//96MHz SDRAM 
		R_MEM_SDRAM_CTRL0 = 0x8010; 
		R_MEM_SDRAM_CTRL1 = 0x2000;                // 0xD0200044 
		R_MEM_SDRAM_TIMING = 0x0F8A;        // 0xD0200048 
		R_MEM_SDRAM_CBRCYC = 0x05D9;        // 0xD020004C 
		R_MEM_SDRAM_MISC = 0x0006;                // 0xD0200050 
		R_MEM_DRV = 0x5555;                                // 0xC0000124                // 0x0000=4mA, 0x5555=8mA, 0xAAAA=12mA 
	}
#endif

	// Clear reset flags
	R_SYSTEM_RESET_FLAG = 0x0000001C;

  	R_SYSTEM_CTRL =0x000003A2;// 0x000003A3;
  
	if (cpu_clock_mhz > 6) {
		while (R_SYSTEM_POWER_STATE == 0) ;
		
        if ((*((volatile INT32U *) 0xF0001FFC))==0x312E3476) {
        	// A edition IC
		    if (cpu_clock_mhz >= 118) {
				R_SYSTEM_PLLEN = 0x16;
				R_SYSTEM_PLLEN |= 0x40;
			} else if (cpu_clock_mhz >= 113) {
				R_SYSTEM_PLLEN = 0x15;
				R_SYSTEM_PLLEN |= 0x40;
			} else { 
		    	R_SYSTEM_PLLEN = (cpu_clock_mhz/3) & 0x0000003F;
		    }
        } else if ((*((volatile INT32U *) 0xF0001FFC))==0x322E3576) {
        #if 0
             // B edition IC
             if (cpu_clock_mhz >= 115) {
				R_SYSTEM_PLLEN = 0x18;
				R_SYSTEM_PLLEN |= 0x40;
			} else if (cpu_clock_mhz >= 110) {
				R_SYSTEM_PLLEN = 0x17;
				R_SYSTEM_PLLEN |= 0x40;
			} else if (cpu_clock_mhz >= 105) {
				R_SYSTEM_PLLEN = 0x16;
				R_SYSTEM_PLLEN |= 0x40;
			} else if (cpu_clock_mhz >= 100) {
				R_SYSTEM_PLLEN = 0x15;
				R_SYSTEM_PLLEN |= 0x40;
			} else {
                R_SYSTEM_PLLEN = (cpu_clock_mhz*3 >> 3) & 0x0000003F;
	        }
			#else
			 if (1)
	        {
				//R_SYSTEM_PLLEN = 0x14;	// 96.0 MHz
				//R_SYSTEM_PLLEN = 0x19;	//120.0 MHz
				//R_SYSTEM_PLLEN = 0x1A;	//124.8 MHz
				R_SYSTEM_PLLEN = 0x1B;	//129.6 MHz
				//R_SYSTEM_PLLEN = 0x1C;	//134.4 MHz
				//R_SYSTEM_PLLEN = 0x1D;	//139.2 MHz
				//R_SYSTEM_PLLEN = 0x1e;	//144.0 MHz
				R_SYSTEM_PLLEN |= 0x40;
			}
			 #endif
			
        }
		// Enable fast PLL
		if (cpu_clock_mhz > 48) {
			// Set USB and SPU clock to PLL/2 when system clock is faster than 48MHz
			R_SYSTEM_CLK_CTRL = 0x00009008;
		} else {
			R_SYSTEM_CLK_CTRL = 0x00009000;
		}
	} else {
		// Set fast PLL to 24MHz
		R_SYSTEM_CLK_CTRL = 0x00001000;
	}
    //R_SYSTEM_PLLEN |= 0x00000100;
	//R_SYSTEM_PLLEN &= ~0x00000100;		// Use 32K clock divided from 24M clock

    __asm {
    	NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	    NOP
	};

    while (R_SYSTEM_POWER_STATE == 0) ; 			// Wait stable
    
	// Initiate low voltage reset function: enable LVR, set core power to 1.8V
	#if USED_MCU_NAME != MCU_GPDV6655D
	R_SYSTEM_LVR_CTRL = 0;
	#endif
	
	// Initiate watch dog
	R_SYSTEM_WATCHDOG_CTRL = 0;      
}

void system_bus_arbiter_init(void)
{
	/*
	0 ppu display
	1 sensor
	2 usb20
	3 ppu engine
	4 dma
	5 jpg
	6 scal
	7 spu
	8 nfc
	9 cpu
	a mp3
	b mp4
	*/
	R_MEM_M2_BUS_PRIORITY =0;
	R_MEM_M3_BUS_PRIORITY =0;
	R_MEM_M4_BUS_PRIORITY =2;
	R_MEM_M5_BUS_PRIORITY =0;
	R_MEM_M6_BUS_PRIORITY =0;
	R_MEM_M7_BUS_PRIORITY =0;
	R_MEM_M8_BUS_PRIORITY =0;
	R_MEM_M9_BUS_PRIORITY =0; //1	//20160325
	R_MEM_M10_BUS_PRIORITY =0;
	R_MEM_M11_BUS_PRIORITY =0;
}

extern INT32U    day_count;
INT32U system_rtc_counter_get(void)
{
    return (INT32U) day_count;
}

void system_rtc_counter_set(INT32U rtc_cnt)
{
    day_count = rtc_cnt;
}

/* place to internal ram (432byte)*/
#ifndef __CS_COMPILER__
#pragma arm section rwdata="pwr_ctrl", code="pwr_ctrl"
#else
void system_power_on_ctrl(void) __attribute__ ((section(".pwr_ctrl")));
void system_enter_halt_mode(void) __attribute__ ((section(".pwr_ctrl")));
void sys_ir_delay(INT32U t) __attribute__ ((section(".pwr_ctrl")));
#endif
INT32U    day_count = 0xFFFFFFFF;//2454829; /* julian date count */
INT32U    halt_data = 1;
INT32U    ir_opcode = 1;
INT32U	  Alarm_Trriger_By_Gp6 = 1;	
INT32U	  Day_Trriger_By_Gp6 = 1;

void sys_ir_delay(INT32U t)
{
	R_TIMERB_PRELOAD = (0x10000-t);

	R_TIMERB_CTRL |= 0x2000;
	while((R_TIMERB_CTRL & 0x8000) == 0);
  	R_TIMERB_CTRL &= ~0x2000;
}

void system_power_on_ctrl(void)
{
	INT32U  i;

	/* wakeup by RTC_DAY int */
	#if _DRV_L1_RTC == 1
	if (R_RTC_INT_STATUS & RTC_DAY_IEN) {
		R_RTC_INT_STATUS = 0x1f;
		day_count++; /* add 1 to julian date conut */
		if (ir_opcode != 0xFFFF0000) {
			system_enter_halt_mode();
		}
	}
	#endif
	
	if (ir_opcode != 0xFFFF0000) {	
		/* wakeup by extab */
		if (R_INT_IRQFLAG & INT_EXTAB) {
		  	//R_TIMERB_PRELOAD = 0xFFF8; /* 1 msec */
			//R_TIMERB_PRELOAD = 0xF999; /* 200 msec */
			//R_TIMERB_CTRL = 0x8063;/*8192 (122us per-count)*/
			R_TIMERB_CTRL = 0x8061;
			R_INT_KECON |= 0x40; /* clear exta int */
		 
			/* default IOF5 is input with pull low */
			//R_IOF_DIR &= ~0x20; /* IOF5 */
		  	sys_ir_delay(7519); /* 10 ms in 48MHz*/
		  	//sys_ir_delay(22557);
		  	for (i=0;i<22;i++) {
				if ((R_IOF_I_DATA & 0x20) == 0) {
					system_enter_halt_mode();
				}
				sys_ir_delay(752); /* 1 ms in 48MHz*/
			}
				
		    #if 0
  			cnt =0;
    		for (i=0; i<=200; i++) { /* press at least 200ms */
				if (cnt >= 10) {
					system_enter_halt_mode();
				}
    	
  				R_TIMERB_CTRL |= 0x2000;
  				while((R_TIMERB_CTRL & 0x8000) == 0);
  				R_TIMERB_CTRL &= ~0x2000;
    	
				if ((R_IOF_I_DATA & 0x20) == 0) {
					cnt++;
					i = 0;
				}
			}
		    #endif
		}
    }
	//R_MEM_SDRAM_CTRL0 |= 0x10; /* enable SDRAM */
	R_SYSTEM_CTRL &= ~0x20; /* strong 6M mode */
}

void system_enter_halt_mode(void)
{
	//R_IOF_ATT |= 0x0010;
	//R_IOF_DIR |= 0x0010;
	//R_IOF_O_DATA &= ~0x10;
	//R_MEM_M11_BUS_PRIORITY |= 1;
	R_SYSTEM_PLL_WAIT_CLK = 0x100; /* set pll wait clock to 8 ms when wakeup*/
	//R_MEM_SDRAM_CTRL0 &= ~0x10; /* disable SDRAM */
	R_SYSTEM_CTRL &= ~0x2; /* CPU reset when wakeup */

    R_SYSTEM_HALT = 0x500A;
    halt_data = R_CACHE_CTRL;

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

//================================================================================
// 2016-04-26 liuxi
#if USED_MCU_NAME == MCU_GPDV6655D 
INT32S sys_pwr_key0_read(void)
{
	return (R_SYSTEM_POWER_CTRL1 & 0x10);
}

INT32S sys_pwr_key1_read(void)
{
	return (R_SYSTEM_POWER_CTRL1 & 0x20);
}

void sys_ldo12_ctrl(int voltage)
{
	// turn on/off LDO codec
	int reg = R_SYSTEM_LDO_CTRL;

	reg &= (~0x0070);
	reg |= voltage;
	R_SYSTEM_LDO_CTRL = reg;
}

void sys_ldo33_ctrl(int enable, int voltage)
{
	// turn on/off LDO codec
	int reg = R_SYSTEM_LDO_CTRL;

	reg &= (~0x0C00);
	reg |= voltage;
	R_SYSTEM_LDO_CTRL = reg;
	
	if (enable) {
		R_SYSTEM_POWER_CTRL1 |= 0x01;		// Enable 3.3V LDO
	}
	else {
		R_SYSTEM_POWER_CTRL1 &= ~0x01;		// Disable 3.3V LDO
	}
}

void sys_ldo28_ctrl(int enable, int voltage)
{
	// turn on/off sensor LDO
	int reg;
	
	reg = (R_SYSTEM_LDO_CTRL & (~0x1300)) | voltage;
	if (enable) {
		R_SYSTEM_LDO_CTRL = 0x1000 | reg;
	}
	else {
		R_SYSTEM_LDO_CTRL = reg;
	}
}
#endif
//================================================================================

#ifndef __CS_COMPILER__
#pragma arm section rwdata, code
#endif

void sys_ir_opcode_clear(void)
{
	ir_opcode = 0;	
}

INT32U sys_ir_opcode_get(void)
{
	return ir_opcode;	
}

INT32S sys_sdram_MHZ_set(SDRAM_CLK_MHZ sdram_clk)
{

    INT32U cbrcyc_reg_val;
    INT32U oc=1; /* Internal test for overing clock */

    //cbrcyc_reg_val = R_MEM_SDRAM_CBRCYC;

    cbrcyc_reg_val=(refresh_period*sdram_clk/10)&0xFFFF * oc;

    R_MEM_SDRAM_CBRCYC = cbrcyc_reg_val;

    return STATUS_OK;
}

void sys_weak_6M_set(BOOLEAN status)
{
	if (status == TRUE) {
		R_SYSTEM_CTRL |= 0x20; /* weak 6M mode */
	}
	else {
		R_SYSTEM_CTRL &= ~0x20; /* strong 6M mode */
	}
}

void sys_reg_sleep_mode_set(BOOLEAN status)
{
	if (status == TRUE) {
		R_SYSTEM_CTRL |= 0x80; /* enable reg sleep mode */
	}
	else {
		R_SYSTEM_CTRL &= ~0x80; /* disable reg sleep mode */
	}
}

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#endif //(defined _DRV_L1_SYSTEM) && (_DRV_L1_SYSTEM == 1)        //
//================================================================//
