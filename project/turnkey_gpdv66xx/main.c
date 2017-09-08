#include "main.h"

extern volatile void *NorWakeupVector;
extern volatile void *Wakeup;
extern void Secrecy_IO_Init(void);	//初始化I2C

#define PERIPHERAL_HANDLING_STACK_SIZE		128
#define STATE_HANDLING_STACK_SIZE			192
//#define DISPLAY_TASK_STACK_SIZE				1024
#define STORAGE_SERVICE_TASK_STACK_SIZE		512
#define USB_TASK_STACK_SIZE					512
#define IDLE_TASK_STACK_SIZE				32

INT32U PeripheralHandlingStack[PERIPHERAL_HANDLING_STACK_SIZE];
INT32U StateHandlingStack[STATE_HANDLING_STACK_SIZE];
#if C_VIDEO_PREVIEW == CUSTOM_ON
	INT32U DisplayTaskStack[DISPLAY_TASK_STACK_SIZE];
#endif	
INT32U StorageServiceTaskStack[STORAGE_SERVICE_TASK_STACK_SIZE];
INT32U USBTaskStack[USB_TASK_STACK_SIZE];
INT32U IdleTaskStack[IDLE_TASK_STACK_SIZE];

void idle_task_entry(void *para)
{
	OS_CPU_SR cpu_sr;
	INT16U i;
	while (1) {
OS_ENTER_CRITICAL();
		i=0x5005;
		R_SYSTEM_WAIT = i;
		i = R_CACHE_CTRL;

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
OS_EXIT_CRITICAL();	    
	}
}

void Main(void *free_memory)
{
	INT32U free_memory_start, free_memory_end;
	//=======================================================================================
	#if (USED_MCU_NAME != MCU_GPDV6655B)
      INT32U trim; 
      
      (*((volatile INT32U *) 0xD000008C)) |= 0x0080; 
      trim = (*((volatile INT32U *) 0xD000008C)) ^ 0x007F; 
     // (*((volatile INT32U *) 0xD000008C)) &= ~0x00FF; 
      (*((volatile INT32U *) 0xD000008C)) &= ~0x0080; 
      (*((volatile INT32U *) 0xD000008C)) &= ~0x007F; 

      (*((volatile INT32U *) 0xD000008C)) |= trim & 0x007F; 

      (*((volatile INT32U *) 0xD0000084)) = 0x50;                        // Enable OSC 
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
      (*((volatile INT32U *) 0xD0000084)) = 0x52;                        // Switch to OSC 
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
      (*((volatile INT32U *) 0xD0000084)) = 0x42;                        // Disable XCKG 
    #endif
	//=======================================================================================
	
	// Touch those sections so that they won't be removed by linker
	if (!NorWakeupVector && !Wakeup) {
		*((INT32U *) free_memory+1) = 0x0;
	}

	free_memory_start = ((INT32U) free_memory + 3) & (~0x3);	// Align to 4-bytes boundry
	free_memory_end = (INT32U) SDRAM_END_ADDR & ~(0x3);

	// Initiate Operating System
	OSInit();
 /*
 	(*((volatile INT32U *) 0xD0200040)) = 0x5210;
	(*((volatile INT32U *) 0xD0200044)) = 0x2000;
	(*((volatile INT32U *) 0xD0200048)) = 0x0F8A;
	(*((volatile INT32U *) 0xD020004C)) = 0x05D9;
	(*((volatile INT32U *) 0xD0200050)) = 0x0006;
	(*((volatile INT32U *) 0xC0000124)) = 0xAAAA;
	(*((volatile INT32U *) 0xD000000C))|=0x30;////////	clk driving
	//(*((volatile INT32U *) 0xC0000124)) = 0;
*/
	// Initiate drvier layer 1 modules
    drv_l1_init();
 	//----------------------------------
	#if SECRECY_ENABLE
 	Secrecy_IO_Init();	//初始化I2C
	#endif
	//line0 as gpio
 	R_ADC_USELINEIN = 0xA;
	R_ADC_MADC_CTRL &= ~0x7;
	R_ADC_MADC_CTRL |= 0x1;
	//=======================================================================================
	#if (USED_MCU_NAME != MCU_GPDV6655B)
	(*((volatile INT32U *) 0xD000005C)) &= ~0x100;                        // Using 32786Hz clock divided from 24MHz instead of clock from 32768Hz crystal 
	#endif
	//=======================================================================================

	timer_freq_setup(0, OS_TICKS_PER_SEC, 0, OSTimeTick);

	// Initiate driver layer 2 modules
	drv_l2_init();

	// Initiate gplib layer modules
	gplib_init(free_memory_start, free_memory_end);

	// Initiate applications here
	OSTaskCreate(task_peripheral_handling_entry, (void *) 0, &PeripheralHandlingStack[PERIPHERAL_HANDLING_STACK_SIZE - 1], PERIPHERAL_HANDLING_PRIORITY);
	OSTaskCreate(state_handling_entry, (void *) 0, &StateHandlingStack[STATE_HANDLING_STACK_SIZE - 1], STATE_HANDLING_PRIORITY);
#if C_VIDEO_PREVIEW == CUSTOM_ON
	OSTaskCreate(task_display_entry, (void *) 0, &DisplayTaskStack[DISPLAY_TASK_STACK_SIZE - 1], DISPLAY_TASK_PRIORITY);
#endif
	OSTaskCreate(task_storage_service_entry, (void *) 0, &StorageServiceTaskStack[STORAGE_SERVICE_TASK_STACK_SIZE - 1], STORAGE_SERVICE_PRIORITY);
	OSTaskCreate(state_usb_entry, (void *) 0, &USBTaskStack[USB_TASK_STACK_SIZE - 1], USB_DEVICE_PRIORITY);
#if SUPPORT_JTAG == CUSTOM_OFF
	OSTaskCreate(idle_task_entry, (void *) 0, &IdleTaskStack[IDLE_TASK_STACK_SIZE - 1], (OS_LOWEST_PRIO - 2));
#endif
	
	OSStart();
}
