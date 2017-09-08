/*
* Purpose: system initial functions after reset
*
* Author: dominant
*
* Date: 2009/12/08
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.00
* History :
*/
//Include files
#include "driver_l1.h"
#include "drv_l1_system.h"


void drv_l1_init(void)
{
	// First initiate those settings that affect system, like: SDRAM access, system clock, cache, exception handler, interrupt, watchdog
	// Then initiate those hardware modules that are bus masters, like: DMA, JPEG, scaler, PPU, SPU, TFT
	// Finally, initiate other hardware modules
	//================================================================================
	//仅GPDV6655D有内部LDO <20160426  liuxi>
	#if USED_MCU_NAME == MCU_GPDV6655D
		sys_ldo12_ctrl(LDO_12_13V);
	#if LDO_ENABLE == 1
			sys_ldo33_ctrl(1, LDO_33_33V);			// System LDO: On, voltage=3.0V, 3.1V, 3.2V, 3.3V	
  	#else
			sys_ldo33_ctrl(1, LDO_33_30V);			// System LDO: On, voltage=3.0V, 3.1V, 3.2V, 3.3V	
	#endif
		sys_ldo28_ctrl(1, LDO_28_31V);			// Sensor LDO: On, voltage=2.8V, 2.9V, 3.0V, 3.1V
	#endif
	//================================================================================
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#if USED_MCU_NAME != MCU_GPDV6655B
	(*((volatile INT32U *) 0xD0000090)) &= ~0x0F;			// Reset XCKGen
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
	};	
    (*((volatile INT32U *) 0xD0000090)) |= 0x08;			// Enable XCKGen
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
	};
#endif	
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  #if  _DRV_L1_SYSTEM == 1
	system_set_pll(MHZ);				// Set PLL
	system_bus_arbiter_init();			// Initaile bus arbiter
  #endif

  #if _DRV_L1_CACHE == 1
	cache_init();						// Initiate CPU Cache
  #endif

	exception_table_init();				// Initiate CPU exception handler
  #if _DRV_L1_INTERRUPT == 1
	vic_init();							// Initiate Interrupt controller
  #endif

  #if _DRV_L1_WATCHDOG==1
	watchdog_init();					// Initiate WatchDOg
  #endif

  #if _DRV_L1_DMA == 1
	dma_init();							// Initiate DMA controller
  #endif

  #if _DRV_L1_JPEG == 1
	jpeg_init();						// Initiate JPEG engine
  #endif

  #if _DRV_L1_SCALER == 1
	scaler_init();						// Initiate Scaler engine
  #endif

  #if _DRV_L1_GPIO==1
	gpio_init();						// Initiate GPIO and IO PADs
  #endif

  #if _DRV_L1_TFT==1
	tft_init();							// Initiate TFT controller
  #endif

  #if _DRV_L1_UART==1
    uart0_init();	
    uart0_buad_rate_set(UART0_BAUD_RATE);
    
   #if (defined DBG_MESSAGE) && (DBG_MESSAGE==CUSTOM_ON)
	uart0_tx_enable();
   #endif
  #endif

  #if _DRV_L1_SPI==1
	spi_disable(0);						// Initiate SPI
  #endif

  #if _DRV_L1_KEYSCAN==1
	//matre_keyscaninit();
  #endif

  #if _DRV_L1_EXT_INT==1
	ext_int_init();
  #endif

  #if _DRV_L1_DAC==1
	dac_init();						// Initiate DAC
  #endif

  #if _DRV_L1_ADC==1
	adc_init();						// Initiate Analog to Digital Convertor
  #endif

  #if _DRV_L1_TIMER==1
	timer_init();						// Initiate Timer
	timerD_counter_init();              // Tiny Counter Initial (1 tiny count == 2.67 us)
  #endif

  #if _DRV_L1_RTC==1
	rtc_init();							// Initiate Real Time Clock
  #endif

  #if _DRV_L1_SW_TIMER== 1
   #if _DRV_L1_RTC==1
	sw_timer_init(TIMER_RTC, 1024);
   #endif
  #endif
    
  #if (defined SUPPORT_JTAG) && (SUPPORT_JTAG == CUSTOM_OFF)
	gpio_set_ice_en(FALSE);  
  #endif

  #if SDRAM_SIZE == 0x00200000
	R_MEM_IO_CTRL = 0x8C61;			// Release XA12, XA13 and XA14 as GPIO
  #elif SDRAM_SIZE==0x00800000 || SDRAM_SIZE==0x01000000
  	R_MEM_IO_CTRL = 0x8F61;			// Release XA12 as GPIO
  #else
  	R_MEM_IO_CTRL = 0x8FE1;
  #endif
	R_SYSTEM_CLK_EN0 = 0x86BF;	// Disable DAC clock
	R_SYSTEM_CLK_EN1 = 0xDC9C;
	
}
