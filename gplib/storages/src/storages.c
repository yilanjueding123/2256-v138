/*
* Purpose: Storages polling and Flash access
*
* Author: wschung
*
* Date: 2008/11/19
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.06
* History :
*         1 2008/06/06 Created by wschung.
          2 Add UHOST initial detection
          3 IOG[5] as SD/MS/XD card detection pin. If card pin changed ,polling each stroage
          4 IOG[2] as CF card detection pin. If card pin changed ,polling each stroage
          5 uninitial storage after polling.
          6.Add Nand initial
          7.usbh pluginout detection mov to system task
          8.Add Pin detection before storage_init()
          9.Modify storage_sdms_detection() for xdc 
          10.Add Xdc and uhost detection pin (IOH1)
           
* Note    : (USB Host using interrupt)
*/
#include "storages.h"

#define C_BKCS1_PIN  81 //IOF1
#define C_BKCS2_PIN  82 //IOF2
#define C_WP_D_PIN       IO_A8//IO_G6//102//IOG[6]
#define C_STORAGE_CD_PIN IO_G5//101//IOG[5]
#define C_CFC_RST_PIN     97//IOG[1]
#define C_CFC_CD_PIN      98//IOG[2]
#define C_DATA3_PIN       38//IOC[6]
#define C_DATA1_PIN       40//IOC[8]
#define C_DATA2_PIN       41//IOC[9]

#ifndef  USBH_DETECTION_MODE
  #define USBH_DETECTION_MODE USBH_DP_D
#endif

#define MSG_STOR_PLUG_IN              0x10000506
#define MSG_STOR_PLUG_OUT             0x10000507

INT8U   s_usbh_isq_lock = 0;
INT16U  s_state;
INT16U  storage_ststbl[8];
INT8U   s_other_cd_pin;    // CD PIN STATE
INT8U   s_cf_cd_pin;       // CD PIN STATE
INT8U   s_usbd_pin;        // CD PIN STATE
INT8U   s_usbh_dpdm;       // usbh signal
INT8U   s_card_work;       // usbh signal

#if USB_NUM >= 1  
#if(USBH_DETECTION_MODE == USBH_GPIO_D )
INT8U   s_usbh_pin;        // usbh detection pin
INT8U   s_wait_dp_state =0;
INT16U  s_dp_cnt =0;
INT16U  s_wait_time =0;
#elif(USBH_DETECTION_MODE == USBH_GPIO_D_1 )
INT8U   s_usbh_pin;        // usbh detection pin
INT8U   s_wait_dp_state =0;
INT16U  s_dp_cnt =0;
INT16U  s_wait_time =0;
INT8U   s_detection_state =0;
#endif
#endif

INT8U   s_usbh_usbd_state =0;        //
//INT16U   usbh_debouce;       //
//INT16U   usbh_noise_debouce;       //
//INT16U   usbh_noise_flag;

PIN_BOUCE  stor_pin_bouce[5];
//INT32U  storage_test =0;       //
/*
struct Usb_Storage_Access *UsbReadWrite[5] = {
 &USBD_MS_ACCESS,
 &USBD_SD_ACCESS,
 &USBD_CFC_ACCESS,
 &USBD_NF_ACCESS,
};
*/

struct Usb_Storage_Access *UsbReadWrite[MAX_DISK_NUM] = {
#if NAND1_EN == 1
//Domi1
	&USBD_NF_ACCESS,
#elif (MAX_DISK_NUM>0)
    NULL,
#endif

/* B:\> */
#if NAND2_EN == 1
//#err
	&USBD_NF_ACCESS,
#elif (MAX_DISK_NUM>1)
    NULL,
#endif

/* C:\> */
#if SD_EN == 1
	&USBD_SD_ACCESS,
#elif (MAX_DISK_NUM>2)
    NULL,
#endif

/* D:\> */
#if MSC_EN == 1
	&USBD_MS_ACCESS,
#elif (MAX_DISK_NUM>3)
    NULL,
#endif

/* E:\> */
#if CFC_EN == 1
//Domi4
	&USBD_CFC_ACCESS,
#elif (MAX_DISK_NUM>4)
    NULL,
#endif

/* F:\> */
#if XD_EN == 1
	&USBD_XDC_ACCESS,
#elif (MAX_DISK_NUM>5)
    NULL,
#endif

/* G:\> */
#if USB_NUM >= 1
	NULL,
#elif (MAX_DISK_NUM>6)
    NULL,
#endif

/* H:\> */
#if USB_NUM >= 2
    NULL,
#elif (MAX_DISK_NUM>7)
    NULL,
#endif

/* I:\> */
#if USB_NUM >= 3
	NULL,
#elif (MAX_DISK_NUM>8)
    NULL,
#endif
/* J:\> */
#if USB_NUM >= 4
	NULL,
#elif (MAX_DISK_NUM>9)
    NULL,
#endif

/* K:\> */
#if NAND3_EN == 1
	&FS_NAND3_driver,
#elif (MAX_DISK_NUM>10)
    NULL,
#endif

/* L:\> */
#if NOR_EN == 1
	&FS_NOR_driver,
#elif (MAX_DISK_NUM>11)
    NULL,
#endif
/* M:\> */
#if RAMDISK_EN == 1
	&FS_RAMDISK_driver
#elif (MAX_DISK_NUM>12)
    NULL,
#endif
};

//extern INT16U *buffer_ms;
static void storage_detection_isr(void);
//void storage_polling_null(void);
//void storages_cfc_poweroff(void);
void storage_polling_usbh(void);
//void usb_h_and_d_isr(void);


#if 0

INT8U storage_debouce(INT8U pin,INT32U ioport)
{
  INT16U  current_cd_pin,change_cnt,debounce_cnt;
  INT16U   noise_count;
  current_cd_pin = gpio_read_io(ioport);
  change_cnt  =0;
  noise_count =0;
  if (current_cd_pin != pin) //
  {
    for (debounce_cnt=0; debounce_cnt<=500; debounce_cnt++)
	{
		if (change_cnt >= 500)
		{
			break;
		}
		sw_timer_us_delay(1000);
		current_cd_pin = gpio_read_io(ioport);
        if (current_cd_pin == pin)
		{
		    noise_count++;
			change_cnt++;
			debounce_cnt = 0;
			continue;
		}
	}
  }
  if (noise_count>200) current_cd_pin |=0x80 ; //bit 7 as nois bit
  return current_cd_pin;
}

#else

INT8U storage_debouce(INT8U pin,INT32U ioport,INT16U  pin_id)
{
    INT16U  current_cd_pin;
    current_cd_pin = gpio_read_io(ioport);
	if (current_cd_pin != pin)             //
  	{
       stor_pin_bouce[pin_id].debouce++;
	}
	else
	{
	   if (stor_pin_bouce[pin_id].debouce >=8) //if noise happen  // 62ms
	   {
	      stor_pin_bouce[pin_id].noise_flag   =1; //noise event!!
	      stor_pin_bouce[pin_id].noise_debouce =0;
	   }
       stor_pin_bouce[pin_id].debouce =0;
	}
	if (stor_pin_bouce[pin_id].noise_flag)  stor_pin_bouce[pin_id].noise_debouce++;

    if (stor_pin_bouce[pin_id].debouce ==64) //64 x (1/128)  = 500ms
    {
       if (stor_pin_bouce[pin_id].noise_flag) //clear noise event!!
       {
          stor_pin_bouce[pin_id].noise_flag   =0;
      	  stor_pin_bouce[pin_id].noise_debouce=0;
       }
       stor_pin_bouce[pin_id].debouce =0;
       return current_cd_pin;
    }
    if ( stor_pin_bouce[pin_id].noise_debouce > 102) //800ms
    {
    	stor_pin_bouce[pin_id].noise_flag    =0 ;
     	stor_pin_bouce[pin_id].noise_debouce =0 ;
     	current_cd_pin |=0x80 ; //bit 7 as nois bit
      	return current_cd_pin;
    }
    return pin;
}

#endif

#if USB_NUM >= 1
#if((USBH_DETECTION_MODE == USBH_GPIO_D ) || (USBH_DETECTION_MODE == USBH_GPIO_D_1 ))
INT16U usbh_wait_dp_up(INT16U poling_id)
{
  INT16U ret =0;
  poling_id =(poling_id & 0x00ff);
  if  (poling_id == 0x87)      //start state
  {
    s_wait_dp_state =1;
    s_wait_time     =0;
    s_dp_cnt        =0; 
  }
  else if(poling_id == 0x07)  // end state
  {
    s_wait_dp_state =0;    
    ret = 0x07;
  }
  else                       
  {
   if (s_wait_dp_state ==1)   // wait state
   {
     s_wait_time++;
     if ( USB_Host_Signal())
      s_dp_cnt++;
     else    
      s_dp_cnt=0;      
     if (s_dp_cnt >64)      // over 500ms dp is pulled up
     {
        s_wait_dp_state =0;
        ret = 0x87;  //plug in message
     }     
     if (s_wait_time >1280)  // ((over 10 sec ))The pendriver maybe crash !
     {
        s_wait_dp_state =0;
     }         
   }
   else
   {
     ret = poling_id;     
   }
  }  
  return ret ;  
}
#endif
#endif
/*
void usb_h_and_d_isr(void)
{
   //DBG_PRINT ("usb isr!\r\n");
   if (USBD_PlugIn)
   {
	usb_isr();
	 USBH_INT_CLR_IMMEDIATE();
   }
   else
   {
   if (s_usbh_isq_lock)
     USBH_INT_CLR_IMMEDIATE();
   else
	usbh_isr();
   }
	//usb_isr_clear();
}
*/
#if CFC_EN == 1
void storages_cfc_reset(void)  //PWR ON and OFF 
{
    gpio_write_io(C_DATA3_PIN, 0);
    gpio_write_io(C_DATA1_PIN, 0);
    gpio_write_io(C_DATA2_PIN, 0);
    gpio_write_io(C_CFC_RST_PIN, 1);
    sw_timer_us_delay(200000);
    //sw_timer_ms_delay(200);
    gpio_write_io(C_CFC_RST_PIN, 0);
    sw_timer_us_delay(200000);
}
void storages_cfc_poweroff(void)
{
    gpio_write_io(C_CFC_RST_PIN, 1);//PMOS OFF
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////
//devicetype :bit   0     1     2      3          4         5        6      7
//            type  CF   SDC    MS   UHOST(DP)   USBD    Nandflash   XD    UHOST_PIN
///////////////////////////////////////////////////////////////////////////////////////
void storages_init(INT16U devicetpye )
{
  INT16U i;
  #if USB_NUM >= 1
  INT16U DP_cnt;
  #endif
//#if _OPERATING_SYSTEM == 1
	/* register USB host */
  
//   usbh_detection_init();
   // I/O PIN initial for detection
//#endif
  //GPIO settings
  
  
  //GPIO G[5] as CD PIN
  gpio_init_io(C_STORAGE_CD_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_STORAGE_CD_PIN, 1);
  //GPIO G[2] as CD PIN
  gpio_init_io(C_CFC_CD_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_CFC_CD_PIN, 1);

  //GPIO F[3] as USB device plugin/out detection PIN (INPUT LOW)
  gpio_init_io(C_USBDEVICE_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_USBDEVICE_PIN, 0);
  gpio_write_io(C_USBDEVICE_PIN,0);
#if USB_NUM >= 1  
#if(USBH_DETECTION_MODE == USBH_GPIO_D )
  //GPIO  USB Host  plugin/out detection PIN 
  
  gpio_init_io(C_USBHOST_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_USBHOST_PIN, 1);
 // gpio_write_io(C_USBHOST_PIN,0);  
 //GPIO  USB Host  Power control
  gpio_init_io(C_USBHOST_PWR_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_USBHOST_PWR_PIN, 1);
  gpio_write_io(C_USBHOST_PWR_PIN,1);  
#elif (USBH_DETECTION_MODE == USBH_GPIO_D_1 )  
  gpio_init_io(C_USBHOST_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_USBHOST_PIN, 1);
 // gpio_write_io(C_USBHOST_PIN,0);  
 //GPIO  USB Host  Power control
  gpio_init_io(C_USBHOST_PWR_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_USBHOST_PWR_PIN, 1);
  gpio_write_io(C_USBHOST_PWR_PIN,0);  
#endif


#endif



  //GPIO G[6] as WPB PIN
  gpio_init_io(C_WP_D_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_WP_D_PIN, ATTRIBUTE_LOW);
  gpio_write_io(C_WP_D_PIN,DATA_LOW);
  //GPIO G[2] as CFC rest pin
  gpio_init_io(C_CFC_RST_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_CFC_RST_PIN, 1);
  gpio_write_io(C_CFC_RST_PIN, 0);
  //
  gpio_init_io(C_DATA3_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_DATA3_PIN, 1);
  gpio_init_io(C_DATA1_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_DATA1_PIN, 1);
  gpio_init_io(C_DATA2_PIN,GPIO_OUTPUT);
  gpio_set_port_attribute(C_DATA2_PIN, 1);

  sw_timer_us_delay(15000); /* wait port stable */
  s_other_cd_pin = gpio_read_io(C_STORAGE_CD_PIN);
  s_cf_cd_pin = gpio_read_io(C_CFC_CD_PIN);
  s_usbd_pin  = gpio_read_io(C_USBDEVICE_PIN); 
  
#if (USBH_DETECTION_MODE == USBH_GPIO_D_1 )     
  if (s_usbd_pin)
    s_detection_state = 1;
  else
  {
    gpio_write_io(C_USBHOST_PWR_PIN,1);   
    sw_timer_us_delay(50); 
  }
#endif
  
  //ms_hand_set(!s_other_cd_pin);//send to ms/pro cd status

  //uninital all
  gpio_write_io(C_DATA3_PIN, 1);
  gpio_write_io(C_DATA1_PIN, 1);
  gpio_write_io(C_DATA2_PIN, 1);
  drvl2_sd_card_remove();
  gpio_write_io(C_DATA3_PIN, 0);
  gpio_write_io(C_DATA1_PIN, 0);
  gpio_write_io(C_DATA2_PIN, 0);

#if CFC_EN == 1
  cfc_uninit();
#endif
#if MSC_EN == 1
  ms_uninitial();

  //ms tbl
 if (buffer_ms != NULL){
     gp_free(buffer_ms);
     buffer_ms =NULL;
  }
#endif

  s_state =0;
  for (i=0;i<8;i++)
     storage_ststbl[i] =0;
#if CFC_EN == 1             
 if( (devicetpye &0x01) && (!s_cf_cd_pin) )
  {

       storages_cfc_reset(); 
       storage_ststbl[0] =!(INT16U) cfc_initial();
       cfc_uninit();
  }
#endif
  if (!s_other_cd_pin)
  {
#if SD_EN == 1
	  if (devicetpye &0x02)
	  {
    	 gpio_write_io(C_DATA3_PIN, 1);
	     gpio_write_io(C_DATA1_PIN, 1);
    	 gpio_write_io(C_DATA2_PIN, 1);
	     storage_ststbl[1] = !(INT16U) drvl2_sd_init();
		 drvl2_sd_card_remove();
	  }
#endif
#if MSC_EN == 1
  	  if (devicetpye &0x04)
  	  {
    	 if (!storage_ststbl[1])
      	 {
        	gpio_write_io(C_DATA3_PIN, 0);
     		gpio_write_io(C_DATA1_PIN, 0);
        	gpio_write_io(C_DATA2_PIN, 0);
        	buffer_ms = (INT16U*) gp_malloc(8192*2);
        	if (buffer_ms != NULL){
       			 ms_table_init(buffer_ms);
       			 ms_detective_flag_set(1);
	       		 storage_ststbl[2] = !ms_initial();
	       		 ms_detective_flag_set(0);
            }
        	ms_uninitial();
         	gp_free(buffer_ms);
         	buffer_ms =NULL;
     	 }
      }
#endif
#if XD_EN == 1
  	  if (devicetpye &0x40)
  	  {
    	 if ((!storage_ststbl[1]) && (!storage_ststbl[2]))
      	 {
        	gpio_write_io(C_DATA3_PIN, 0);
     		gpio_write_io(C_DATA1_PIN, 0);
        	gpio_write_io(C_DATA2_PIN, 0);
      	 	//DBG_PRINT ("storage initial start! \r\n");	
      	 	storage_ststbl[6]  = !DrvXD_initial();
      	 	DrvXD_uninit();
         }
    }       
#endif  
  }

#if USB_NUM >= 1
// s_usbh_dpdm = (INT8U) USB_Host_Signal();  
#if(USBH_DETECTION_MODE == USBH_GPIO_D )
  s_usbh_pin = gpio_read_io(C_USBHOST_PIN);  
  if (s_usbh_pin)
  {  
      DP_cnt =0;
	  for(i = 0 ; i <1000; i++)
	  {
	    sw_timer_ms_delay(1); 
    	if ( USB_Host_Signal() )
	       DP_cnt ++;       
        if (DP_cnt >500) break;	//over 500ms       
	  }  
	  if (DP_cnt >500)
	    s_usbh_dpdm =1;
	  else  
	    s_usbh_dpdm =0;
  }else
  {
     s_usbh_dpdm =0;
  } 
#elif(USBH_DETECTION_MODE == USBH_DP_D )
  DP_cnt =0;
  for(i = 0 ; i <1000; i++)
  {
    sw_timer_ms_delay(1); 
   	if ( USB_Host_Signal() )
	       DP_cnt ++;       
    if (DP_cnt >500) break;	 //over 500ms              
  }  
  if (DP_cnt >500)
    s_usbh_dpdm =1;
  else  
    s_usbh_dpdm =0;      
#elif(USBH_DETECTION_MODE == USBH_GPIO_D_1 )    
  if (s_detection_state ==0)
  {
	  s_usbh_pin = gpio_read_io(C_USBHOST_PIN);  
	  if (s_usbh_pin)
	  {  
    	  DP_cnt =0;
		  for(i = 0 ; i <1000; i++)
		  {
	    	sw_timer_ms_delay(1); 
	    	if ( USB_Host_Signal() )
		       DP_cnt ++;       
        	if (DP_cnt >500) break;	//over 500ms       
		  }  
	  	  if (DP_cnt >500)
	      	s_usbh_dpdm =1;
	      else  
	        s_usbh_dpdm =0;
	  }else
      {
	      s_usbh_dpdm =0;
	  } 	      
      s_detection_state =3;	  
  }else
  {
     s_usbh_dpdm =0;
  } 
#endif  
  USB_Host_Handup(s_usbh_dpdm);//send to uhost  DP status    
  if ((devicetpye &0x08) && (s_usbh_dpdm) )
  {
       s_usbh_isq_lock =1;
       //system_usbh_uncheat();
       storage_ststbl[3] = !DrvUSBHMSDCInitial();
       //system_usbh_cheat();
       s_usbh_dpdm       =  storage_ststbl[3];
       //DrvUSBHMSDCUninitial();
  }
  
#if((USBH_DETECTION_MODE == USBH_GPIO_D )||(USBH_DETECTION_MODE == USBH_GPIO_D_1 ))
  if (devicetpye &0x80)
 		 storage_ststbl[7] =  s_usbh_pin ;  
#endif 		   
#endif

  if (devicetpye &0x10)
  {
       storage_ststbl[4] = s_usbd_pin;
  }

#if (NAND1_EN == 1)
  if (devicetpye &0x20)
  {
#if NAND_CS_POS == NF_CS_AS_BKCS1
       gpio_init_io(C_BKCS1_PIN,GPIO_OUTPUT);
       gpio_set_port_attribute(C_BKCS1_PIN, 1);
       gpio_write_io(C_BKCS1_PIN, 1);  
#elif NAND_CS_POS == NF_CS_AS_BKCS2       
       gpio_init_io(C_BKCS2_PIN,GPIO_OUTPUT);
       gpio_set_port_attribute(C_BKCS2_PIN, 1);
       gpio_write_io(C_BKCS2_PIN, 1);  
#endif
       storage_ststbl[5] = !(INT16U)NAND_Initial();
       NAND_Uninitial();
      // nand_flash_uninit();
  }
#endif

  //register interrupter_isq and timer_isr
  USBD_PlugIn = 1;	// added by Bruce, 2010/05/25
  					// to solve the problem which USB device can't be recognized if USB cable is connected before executing USBD demo
  //vic_irq_register(VIC_USB, usb_h_and_d_isr);
  //vic_irq_enable(VIC_USB);
  
  storage_polling_start();
  s_usbh_isq_lock =0;
  s_card_work =0;
  //0:SD 1:USBD 2:CF 3:USBH 4:UHOSH_PIN
  for(i = 0 ; i <5; i++)
  {
    stor_pin_bouce[i].debouce = 0;
    stor_pin_bouce[i].noise_debouce = 0;
    stor_pin_bouce[i].noise_flag = 0;
  }

}

//INT16U htest =0;
//INT16U htest2 =0;

#if (USB_NUM >= 1)
void storage_polling_usbh(void)
{
   INT16U  current_cd_pin;
    current_cd_pin =(INT16U) USB_Host_Signal();
  	if (current_cd_pin != s_usbh_dpdm) // IO detection & debouce 10ms
  	{
       stor_pin_bouce[3].debouce++;
	}
	else
	{
	   if (stor_pin_bouce[3].debouce >=20) //if noise happen  // 62ms
	   {
	      stor_pin_bouce[3].noise_flag   =1; //noise event!!
	      stor_pin_bouce[3].noise_debouce =0;
	   }
       stor_pin_bouce[3].debouce =0;
	}
	if (stor_pin_bouce[3].noise_flag)
	{
 	    USB_Host_Handup(current_cd_pin);
		stor_pin_bouce[3].noise_debouce++;
	}
    if (stor_pin_bouce[3].debouce ==64) //64 x (1/128)  = 500ms
    {
       if (stor_pin_bouce[3].noise_flag) //clear noise event!!
       {
          stor_pin_bouce[3].noise_flag   =0;
      	  stor_pin_bouce[3].noise_debouce=0;
       }
       stor_pin_bouce[3].debouce =0;
       s_usbh_dpdm =  current_cd_pin ;
	   storage_ststbl[3] = current_cd_pin;
       s_usbh_dpdm  = current_cd_pin;
       USB_Host_Handup(s_usbh_dpdm);
   }
}
#endif

#if 0
void storage_polling_null(void)
{
	sys_release_timer_isr(storage_detection_isr);
}
#endif
void storage_polling_start(void)
{
	sys_registe_timer_isr(storage_detection_isr);
}

void storage_polling_stop(void)
{
	sys_release_timer_isr(storage_detection_isr);
}

INT16U storage_polling(void)
{
  INT8U  current_cd_pin;
  INT16U result;

  result =0;
  //polling SDC PIN
  current_cd_pin = storage_debouce(s_other_cd_pin,C_STORAGE_CD_PIN,0);
  if((current_cd_pin & 0x0f) != s_other_cd_pin)
  {
    current_cd_pin &=0x7F;
   	result=1;
    s_other_cd_pin  = current_cd_pin;
   	if (!current_cd_pin)
       	   result |= 0x80;
#if MSC_EN == 1       	   
    ms_hand_set(!current_cd_pin);
#endif           	   
    return result;
  }else if(current_cd_pin & 0x80) //if noise
  {
    current_cd_pin &=0x7F;
    s_other_cd_pin  = current_cd_pin;
   	result=0x41;   //noise process
   	if (!s_other_cd_pin)
       	   result |= 0x80;   //IN
    else
           result =   0;
     return result;
  }
#if MSC_EN == 1       	
  if (stor_pin_bouce[0].noise_flag)
      ms_hand_set(!current_cd_pin);
#endif
  
  //polling CF PIN
#if CFC_EN == 1
  current_cd_pin = storage_debouce(s_cf_cd_pin,C_CFC_CD_PIN,2);
  if ((current_cd_pin & 0x0f) != s_cf_cd_pin)
  {
    current_cd_pin &=0x7F;
    result=2;
    s_cf_cd_pin  = current_cd_pin;
    if (!current_cd_pin)
           result |= 0x80;
    return result;
  }else if(current_cd_pin & 0x80) //if noise
  {
    current_cd_pin &=0x7F;
    s_cf_cd_pin  = current_cd_pin;
   	result=0x42;    //noise process
   	if (!s_cf_cd_pin)
       	   result |= 0x80;
    else
           result =   0;
     return result;
  }
#endif

#if(USBH_DETECTION_MODE == USBH_GPIO_D )
/////////////////////////////////////////////////////////////////////////////////////
  //polling USBD PIN
 // if ( !s_usbh_pin) 
  current_cd_pin = storage_debouce(s_usbd_pin,C_USBDEVICE_PIN,1);
  if ((current_cd_pin & 0x0f) != s_usbd_pin)
  {
    	current_cd_pin &=0x7F;
		result=4;
	    storage_ststbl[4] = current_cd_pin;
 //   	s_usbd_pin  = current_cd_pin;
	    if (current_cd_pin)
    	     result |= 0x80;   
#if USB_NUM >= 1    	      	     
        s_usbh_pin = storage_debouce(s_usbh_pin,C_USBHOST_PIN,4);
    	storage_ststbl[7] = s_usbh_pin;
    	if ( s_usbh_pin)
    	{
    	  if (s_usbh_usbd_state ==1)
    	  {  
    	    if (result ==0x04)   	         
    	    {
    	      s_usbh_usbd_state =0;
     	      result =0x87; //inint uhost    	  
     	    }
    	    else
      	      result =0;
    	  }else        	  
    	  {
    	    s_usbh_usbd_state = 1;
    	    result =0x07; //remove uhost    	  
    	  }    	  
    	}else{    	    	   
    	   if ((result ==0x04) && (s_usbh_usbd_state ==1)) 
    	   {
  	       	 	result =0;
    	    	s_usbh_usbd_state =0;         	  
    	    }
      	} 
#endif      	
	    return result;
  }else if(current_cd_pin & 0x80) //if noise
  {
    	current_cd_pin &=0x7F;
	    storage_ststbl[4] = current_cd_pin;
 //   	s_usbd_pin  = current_cd_pin;
	   	result=0x44;    //noise process
   		if (s_usbd_pin)
       		   result |= 0x80;
	    else
    	       result =   0;    
#if USB_NUM >= 1     	       	           	           	       
        s_usbh_pin = storage_debouce(s_usbh_pin,C_USBHOST_PIN,4);
    	storage_ststbl[7] = s_usbh_pin;    	    	
    	if ( s_usbh_pin)
    	{
    	  if (s_usbh_usbd_state ==1)
    	  {  
      	      result =0;
    	  }else        	  
    	  {
    	      s_usbh_usbd_state = 1;
    	      result =0x07; //remove uhost    	  
    	  }    	  
    	}  	  
#endif    	  	       
    	return result;
  }
#if USB_NUM >= 1
    //polling USBHOST PIN    
  current_cd_pin = storage_debouce(s_usbh_pin,C_USBHOST_PIN,4);
  if ((current_cd_pin & 0x0f) != s_usbh_pin)
  {
    current_cd_pin &=0x7F;
    result=7;
    storage_ststbl[7] = current_cd_pin;
    s_usbh_pin  = current_cd_pin;    
    if (current_cd_pin)
         result |= 0x80;         
         
    if ((s_usbh_usbd_state ==0) && (result ==0x87) && (s_usbd_pin) )
    {
        s_usbh_usbd_state =1;
        result =0x04; //remove usbd
    }
    else if (s_usbh_usbd_state ==1)
    {    
  //       s_usbd_pin =storage_debouce(s_usbd_pin,C_USBDEVICE_PIN,1);
         storage_ststbl[4] =s_usbd_pin;
         if (!s_usbd_pin)
                 s_usbh_usbd_state =0;         
         result=0;                         
    }         
    return result;
  }else if(current_cd_pin & 0x80) //if noise
  {
    current_cd_pin &=0x7F;
    storage_ststbl[7] = current_cd_pin;
    s_usbh_pin  = current_cd_pin;
   	result=0x67;    //noise process
   	if (s_usbh_pin)
       	   result |= 0x80;
    else
           result =   0;
           
    if ((s_usbh_usbd_state ==0) && (result ==0x87) && (s_usbd_pin) )
    {
        s_usbh_usbd_state =1;
        result =0x04; //remove usbd
    }                                  
    else if (s_usbh_usbd_state ==1)
    {    
 //        s_usbd_pin =storage_debouce(s_usbd_pin,C_USBDEVICE_PIN,1);
         storage_ststbl[4] =s_usbd_pin;
         if (!s_usbd_pin)
                 s_usbh_usbd_state =0;         
         result=0;                         
    } 
    return result;
  }
  
#endif
/////////////////////////////////////////////////////////////////////////////////////
#elif(USBH_DETECTION_MODE == USBH_GPIO_D_1)

 switch (s_detection_state)
 {
   case 1: //for Device
        current_cd_pin = storage_debouce(s_usbd_pin,C_USBDEVICE_PIN,1);
        if ((current_cd_pin & 0x0f) != s_usbd_pin)
        {
            current_cd_pin &=0x7F;
		    result=4;
		    storage_ststbl[4] = current_cd_pin;
//		    s_usbd_pin  = current_cd_pin;
		    if (current_cd_pin)
		    {
        	 result |= 0x80;   
        	}
        	else      	         	 
        	{
        	 s_detection_state =2; 
        	}
		    return result;           
		    
        }else if(current_cd_pin & 0x80) //if noise
        {
		    current_cd_pin &=0x7F;
		    storage_ststbl[4] = current_cd_pin;
//		    s_usbd_pin  = current_cd_pin;
		   	result=0x44;    //noise process
		   	if (s_usbd_pin)
       		   result |= 0x80;
		    else
		    {
        	   result =   0;           	 
               s_detection_state =2; 
            }
		    return result;        
        }else if (current_cd_pin==0) 
        {
         s_detection_state =2; 
        }
        break;
   case 2://open uhost pwd
        gpio_write_io(C_USBHOST_PWR_PIN,1);  
        s_detection_state =3; 
        return 0;    
        break;   
   case 3://for uhost 
    	current_cd_pin = storage_debouce(s_usbh_pin,C_USBHOST_PIN,4);
		if ((current_cd_pin & 0x0f) != s_usbh_pin)
		{
		    current_cd_pin &=0x7F;
		    result=7;
		    storage_ststbl[7] = current_cd_pin;
		    s_usbh_pin  = current_cd_pin;    
		    if (current_cd_pin)
		    {
        		 result |= 0x80;                  
            }
            else      	         	 
        	{
	        	 s_detection_state =4; 
        	}		 
		    return result;
		}else if(current_cd_pin & 0x80) //if noise
		{
  		    current_cd_pin &=0x7F;
		    storage_ststbl[7] = current_cd_pin;
		    s_usbh_pin  = current_cd_pin;
		   	result=0x67;    //noise process
		   	if (s_usbh_pin)
		   	{
       			   result |= 0x80;
       	    }
		    else
		    {
        		   result =   0;   
 				   s_detection_state =4;         		           
            }
	  		return result;
		}
        else if (current_cd_pin==0) 
        {
           s_detection_state =4; 
        }
        break;
   case 4://close uhost pwd
        gpio_write_io(C_USBHOST_PWR_PIN,0);  
        s_detection_state =1; 
        return 0;            
   default:
        break;         
 }
/////////////////////////////////////////////////////////////////////////////////////
#elif(USBH_DETECTION_MODE == USBH_DP_D )

  //polling USBD
  current_cd_pin = storage_debouce(s_usbd_pin,C_USBDEVICE_PIN,1);
  if ((current_cd_pin & 0x0f) != s_usbd_pin)
  {
    current_cd_pin &=0x7F;
    result=4;
    storage_ststbl[4] = current_cd_pin;
 //   s_usbd_pin  = current_cd_pin;
    if (current_cd_pin)
         result |= 0x80;
    return result;
  }else if(current_cd_pin & 0x80) //if noise
  {
    current_cd_pin &=0x7F;
    storage_ststbl[4] = current_cd_pin;
 //   s_usbd_pin  = current_cd_pin;
   	result=0x44;    //noise process
   	if (s_usbd_pin)
       	   result |= 0x80;
    else
           result =   0;
     return result;
  }

///////////////////////////////////////////////////////////////////////////////////// 
#endif

#if USB_NUM >= 1
  if (usbh_mount_status())
         return result;
#if(USBH_DETECTION_MODE == USBH_DP_D )
  if (!USBD_PlugIn)
  {
    storage_polling_usbh();
  }
#endif

#endif
  return result;
}
// Polling by system timer
static void storage_detection_isr(void)
{
  INT16U storage_id;

  if (s_card_work) return;

  storage_id = storage_polling();  
#if USB_NUM >= 1  
#if((USBH_DETECTION_MODE == USBH_GPIO_D ) || (USBH_DETECTION_MODE == USBH_GPIO_D_1 ))
     storage_id = usbh_wait_dp_up(storage_id);     
#endif  
#endif
  if (storage_id !=0)
  {

   }
}
// Query by upper layer
void storage_sdms_detection(void)
{
   s_card_work =1;
  if (!s_other_cd_pin)
  {
#if XD_EN == 1
	//XDC
     gpio_write_io(C_DATA3_PIN, 0);		
     gpio_write_io(C_DATA1_PIN, 0);
     gpio_write_io(C_DATA2_PIN, 0);
     if (DrvXD_initial()==0)			
   	 	storage_ststbl[6]  = 1;
   	 DrvXD_uninit();
   	 if (storage_ststbl[6]) {
   	    s_card_work =0;
		return;
	}
#else
    storage_ststbl[6]  = 0;         
#endif

#if SD_EN == 1
 if (!storage_ststbl[6])
   {
    //SDC
    gpio_write_io(C_DATA3_PIN, 1);
    gpio_write_io(C_DATA1_PIN, 1);
    gpio_write_io(C_DATA2_PIN, 1);
    if ( drvl2_sd_init()==0)				
	    storage_ststbl[1] = 1;
	drvl2_sd_card_remove();
	if (storage_ststbl[1]) {
 	    s_card_work =0;
		return;
	}
   }
#else
    storage_ststbl[1]  = 0;
#endif  

#if MSC_EN == 1
	if ((!storage_ststbl[6]) && (!storage_ststbl[1]))
	{
	//MS
	gpio_write_io(C_DATA3_PIN, 0);
    gpio_write_io(C_DATA1_PIN, 0);
    gpio_write_io(C_DATA2_PIN, 0);
    if (buffer_ms == NULL){
       	 buffer_ms = (INT16U*) gp_malloc(8192*2);
    }
    ms_table_init(buffer_ms);
    ms_detective_flag_set(1);
    if (ms_initial() ==0)					
	    storage_ststbl[2] = 1;
	ms_detective_flag_set(0);
    gp_free(buffer_ms);
    buffer_ms =NULL;
    ms_uninitial();
    if (storage_ststbl[2]) {
      s_card_work =0;
	  return;
	}	
	}	
#else
   storage_ststbl[2]  = 0;
#endif

  }
  else
  {
	  storage_ststbl[1]  = 0;
	  storage_ststbl[2]  = 0;
	  storage_ststbl[6]  = 0;
  }
    //others......
  s_card_work =0;    
}

void storage_cfc_detection(void)  
{
  
   s_card_work =1;
  if (!s_cf_cd_pin)
  {
#if CFC_EN == 1
    gpio_write_io(C_DATA3_PIN, 0);
    gpio_write_io(C_DATA1_PIN, 0);
    gpio_write_io(C_DATA2_PIN, 0);
    storages_cfc_reset();//power up
    storage_ststbl[0] = !(INT16U) cfc_initial();
    cfc_uninit();
#else
    storage_ststbl[0] =0;
#endif
  }
  else
  {
#if CFC_EN == 1  
   storages_cfc_poweroff();
#endif   
   storage_ststbl[0] =0;
  }
  s_card_work =0;

}


INT16U storage_detection(INT16U type)
{
  return storage_ststbl[type];
}


void storage_usbd_state_exit(void)
{
 
}


#define STG_CFG_CF      0x0001
#define STG_CFG_SD      0x0002
#define STG_CFG_MS      0x0004
#define STG_CFG_USBH    0x0008
#define STG_CFG_USBD    0x0010
#define STG_CFG_NAND1   0x0020
#define STG_CFG_XD      0x0040
INT16U storage_usbd_detection(void)
{
  INT16U stg_init_val=0;	
  //GPIO F[3] as USB device plugin/out detection PIN (INPUT LOW)
  gpio_init_io(C_USBDEVICE_PIN,GPIO_INPUT);
  gpio_set_port_attribute(C_USBDEVICE_PIN, 0);
  gpio_write_io(C_USBDEVICE_PIN,0);
  sw_timer_us_delay(10);  // wait usb detect pin gpio stable
  //sw
//  s_usbd_pin  = gpio_read_io(C_USBDEVICE_PIN);
  storage_ststbl[4] = s_usbd_pin;
  if (s_usbd_pin) 
  {  
    if(NAND1_EN == 1) {stg_init_val |= STG_CFG_NAND1;}
    if(SD_EN == 1) {stg_init_val |= STG_CFG_SD;}
    if(MSC_EN == 1) {stg_init_val |= STG_CFG_MS;}
    if(USB_NUM >= 1) {stg_init_val |= STG_CFG_USBH;stg_init_val |= STG_CFG_USBD;}
	if(CFC_EN == 1) {stg_init_val |= STG_CFG_CF;}
	if(XD_EN == 1) {stg_init_val |= STG_CFG_XD;}    
	storages_init(stg_init_val);
  }
  return (INT16U)s_usbd_pin;
}

INT32U storage_wpb_detection(void)
{
	volatile INT32U rtnValue = 0;
	rtnValue = gpio_read_io(C_WP_D_PIN);
	return 1;//comment by george//rtnValue;
}
