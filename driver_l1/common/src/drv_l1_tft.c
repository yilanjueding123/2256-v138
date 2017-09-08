/* 
* Purpose: TFT driver/interface
*
* Author: Allen Lin
*
* Date: 2008/5/9
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.00
* History :
*/

//Include files
#include "drv_l1_tft.h"

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#if (defined _DRV_L1_TFT) && (_DRV_L1_TFT == 1)                   //
//================================================================//

INT32U  CSN_n;
INT32U  SCL_n;
INT32U  SDA_n;
INT32U  SDI_n;

void TFT_TXDT240C_5182A_Init(void);

void tft_slide_en_set(BOOLEAN status)
{
	if (status == TRUE) {
		R_TFT_TS_MISC |= TFT_SLIDE_EN;
	}
	else {
		R_TFT_TS_MISC &= ~TFT_SLIDE_EN;
	}
}

void tft_init(void)
{
 	R_TFT_CTRL = 0;
	R_SPI0_CTRL = 0;
	tft_slide_en_set(FALSE);
#if 0
	tft_tpo_td025thea7_init();
#else	
	TFT_TXDT240C_5182A_Init();
#endif	
}

void cmd_delay(INT32U i) 
{
	unsigned int j, cnt;
	cnt = i*2;
	for (j=0;j<cnt;j++);
}

void cmd_longdelay(INT32U i) 
{
	INT32U j, cnt;
	
	cnt = i*100;
	
	for (j=0;j<cnt;j++);	
}

void tft_data_mode_set(INT32U mode)
{
	R_TFT_TS_MISC &= ~TFT_DATA_MODE;
	R_TFT_TS_MISC |= mode;	
}

void tft_mem_unit_set(BOOLEAN status)
{
	if (status == TRUE) {
		R_TFT_CTRL |= TFT_MEM_BYTE_EN;
	}
	else {
		R_TFT_CTRL &= ~TFT_MEM_BYTE_EN; /* word */
	}
}

void tft_mem_mode_ctrl_set(INT32U mem_ctrl)
{
	R_TFT_CTRL &= ~TFT_MODE;
	R_TFT_CTRL |= (mem_ctrl|TFT_EN);		
}

void tft_clk_set(INT32U clk)
{
	R_TFT_CTRL &= ~TFT_CLK_SEL;
	R_TFT_TS_MISC &= ~0xC0;
	
	if (clk < TFT_CLK_DIVIDE_9) {
		R_TFT_CTRL |= clk;
	}
	else {
		R_TFT_CTRL |= clk & 0xF;
		R_TFT_TS_MISC |= (clk & 0x20) << 1;
		R_TFT_TS_MISC |= (clk & 0x10) << 3;
	}	
}	

void i80_write(INT8U index,INT8U *data,INT8U len)
{
	INT32U i;
	
	R_TFT_MEM_BUFF_WR = index;
	tft_mem_mode_ctrl_set(TFT_MODE_MEM_CMD_WR);

	for (i=0;i<len;i++) {
		R_TFT_MEM_BUFF_WR = *data++;
		tft_mem_mode_ctrl_set(TFT_MODE_MEM_DATA_WR);
	}
}
//driver fd54122
void tft_g200_init(void)
{
	INT8U data[16] = {0};
	
	R_TFT_CTRL = 0;
	R_TFT_V_PERIOD = 128-1;
    R_TFT_VS_WIDTH = 5;  //blanking time
    R_TFT_V_START = 5;    //setup time                                                                              
    R_TFT_H_PERIOD = 128-1; 
    R_TFT_HS_WIDTH = 1;   //RD/WR plus low
    R_TFT_H_START = 1;	  //RD/WR plus hi
    
   
	tft_mem_unit_set(TRUE); // Byte mode
	tft_clk_set(TFT_CLK_DIVIDE_2);
	tft_data_mode_set(TFT_DATA_MODE_8);
	
	
	gpio_init_io (IO_F12, GPIO_OUTPUT);
	gpio_set_port_attribute(IO_F12, 1);
	
	drv_msec_wait(5);
	gpio_write_io(IO_F12,0);
	drv_msec_wait(5);
	gpio_write_io(IO_F12,1);
	drv_msec_wait(5);

	i80_write(0x11,0,0); /* sleep out */
	drv_msec_wait(200);
	
	
    data[0] = 0x4;             
	i80_write(0x26,data,1); /* gamma 2.5 */
	
	data[0] = 0xe;
	data[1] = 0x14;
	i80_write(0xb1,data,2); /* frame control */
	
	data[0] = 0x8;
	data[1] = 0x0;
	i80_write(0xc0,data,2); /* GVDD */
	
	data[0] = 0x5;             
	i80_write(0xc1,data,1); //AVDD VGL VGL VCL VGH = 16.5V  VGL=-8.25V 
	
	//data[0] = 0x3a;
	//data[1] = 0x2d;
	data[0] = 0x64;
	data[1] = 0x64;
	i80_write(0xc5,data,2); //SET VCOMH & VCOML	
	
	data[0] = 0x3f;
	i80_write(0xc6,data,1); 
	
	data[0] = 0x5;
	i80_write(0x3A,data,1); //format
	
	
	data[0] = 0x00;
	i80_write(0xb4,data,1); //all line inversion

	data[0] = 0xc8; 
	i80_write(0x36,data,1); /* pixel odred is BGR */
	
	data[0] = 0;
	data[1] = 2;
	data[2] = 0;
	data[3] = 129;
	
	i80_write(0x2a,data,4); /* column address range */
	
	data[0] = 0;
	data[1] = 1+2;
	data[2] = 0;
	data[3] = 128+2;
	
	i80_write(0x2b,data,4); /* row address range */
	
	i80_write(0x29,0,0); /* display on */
	
	
	R_TFT_MEM_BUFF_WR = 0x2C;
    tft_mem_mode_ctrl_set(TFT_MODE_MEM_CMD_WR);
    
    R_TFT_CTRL = 0x20d3; //byte mode and update once
}

void tft_mem_buf_update(void)
{
	//R_TFT_MEM_BUFF_WR = 0x2C;
    //tft_mem_mode_ctrl_set(TFT_MODE_MEM_CMD_WR);
	R_TFT_CTRL = 0x20d3;	
}

void tft_tft_en_set(BOOLEAN status)
{
	if (status == TRUE) {
		R_TFT_CTRL |= TFT_EN;
	}		
	else {
		R_TFT_CTRL &= ~TFT_EN;
	}
}

void tft_mode_set(INT32U mode)
{
	R_TFT_CTRL &= ~TFT_MODE;
	R_TFT_CTRL |= mode;		
}

void tft_signal_inv_set(INT32U mask, INT32U value)
{
	/*set vsync,hsync,dclk and DE inv */
	R_TFT_CTRL &= ~mask;
	R_TFT_CTRL |= (mask & value);		
}

void serial_cmd_1(INT32U cmd) {

	INT32S i;
	
	gpio_write_io(CSN_n,1);//CS=1
	gpio_write_io(SCL_n,0);//SCL=0
	gpio_write_io(SDA_n,0);//SDA
	
	// set csn low
	gpio_write_io(CSN_n,0);//CS=0
	cmd_delay(1);
	for (i=0;i<16;i++) {
		// shift data
		gpio_write_io(SDA_n, ((cmd&0x8000)>>15)); /* SDA */
		cmd = (cmd<<1);
		cmd_delay(1);
		// toggle clock
		gpio_write_io(SCL_n,1);//SCL=0
		cmd_delay(1);
		gpio_write_io(SCL_n,0);//SCL=0		
		cmd_delay(1);
	}
	
	// set csn high
	gpio_write_io(CSN_n,1);//CS=1
	
	cmd_delay(1);
			
}

void tft_tpo_td025thea7_init(void)
{
	/* 320*240 */
#if 0
	INT32U spi_cs = IO_E0;
	INT8U send[2];
	
	spi_cs_by_internal_set(SPI_0,spi_cs, 0);
	spi_clk_set(SPI_0,SYSCLK_8); 
	spi_pha_pol_set(SPI_0,PHA0_POL0);
	
	send[0] = 0x08;
	send[1] = 0;
	spi_transceive(SPI_0,send, 2, NULL, 0);
#else
#if GPDV_BOARD_VERSION == GPDV_EMU_V2_0
	CSN_n = IO_G10;
	SDA_n = IO_G11;
    SCL_n = IO_C11;
#else
	CSN_n = IO_G15;
	SDA_n = IO_G14;
    SCL_n = IO_G13;
#endif
	
    gpio_init_io (CSN_n, GPIO_OUTPUT);
	gpio_init_io (SCL_n, GPIO_OUTPUT);
	gpio_init_io (SDA_n, GPIO_OUTPUT);
	
	gpio_set_port_attribute(CSN_n, 1);
	gpio_set_port_attribute(SCL_n, 1);
	gpio_set_port_attribute(SDA_n, 1);
	
    serial_cmd_1(0x0800);
#endif
	/*
	R_TFT_LINE_RGB_ORDER = 0x00;
	R_TFT_V_PERIOD = 262;
	R_TFT_V_START = 22-1;
	R_TFT_V_END	= 22+240-1;
	
	R_TFT_H_PERIOD = 1560;
	R_TFT_H_START = 240;
	R_TFT_H_END	= 240+1280;
	*/
	R_TFT_HS_WIDTH			= 0;				//	1		=HPW
	R_TFT_H_START			= 1+239;			//	240		=HPW+HBP
	R_TFT_H_END				= 1+239+1280;	//	1520	=HPW+HBP+HDE
	R_TFT_H_PERIOD			= 1+239+1280+40;	//	1560	=HPW+HBP+HDE+HFP
	R_TFT_VS_WIDTH			= 0;				//	1		=VPW				(DCLK)
	R_TFT_V_START			= 21;			//	21		=VPW+VBP			(LINE)
	R_TFT_V_END				= 21+240;		//	261		=VPW+VBP+VDE		(LINE)
	R_TFT_V_PERIOD			= 21+240+1;		//	262		=VPW+VBP+VDE+VFP	(LINE)
	R_TFT_LINE_RGB_ORDER    = 0x00;
	
	tft_signal_inv_set(TFT_VSYNC_INV|TFT_HSYNC_INV, (TFT_ENABLE & TFT_VSYNC_INV)|(TFT_ENABLE & TFT_HSYNC_INV));
	tft_mode_set(TFT_MODE_UPS052);
	tft_data_mode_set(TFT_DATA_MODE_8);
	tft_clk_set(TFT_CLK_DIVIDE_5); /* FS=66 */
#if 0
	spi_cs_by_external_set(SPI_0);
	spi_clk_set(SPI_0,SYSCLK_8); 
#endif
	//tft_tft_en_set(TRUE);
}

void TPO_SPI_WriteCmd1 (INT32S nCmdType, INT32U uCmd)
{
	INT32S	nBits;
	INT32S	i;
	
	CSN_n = IO_G11;
	SDA_n = IO_C2;
    SCL_n = IO_C3;

	//	Initial 3-wire GPIO interface, Initial SPI
	gpio_set_port_attribute(SDA_n, ATTRIBUTE_HIGH);
	gpio_set_port_attribute(SCL_n, ATTRIBUTE_HIGH);
	gpio_set_port_attribute(CSN_n, ATTRIBUTE_HIGH);
	gpio_init_io(SDA_n, GPIO_OUTPUT);					 	
	gpio_init_io(SCL_n, GPIO_OUTPUT);
	gpio_init_io(CSN_n, GPIO_OUTPUT);	
	gpio_write_io (SDA_n, DATA_LOW);
	gpio_write_io (SCL_n, DATA_LOW);
	gpio_write_io (CSN_n, DATA_HIGH);
	cmd_delay (5);
	
	nBits = (nCmdType + 2) << 3;
	uCmd &= ~((((INT32U)1) << (nBits - 9 * nCmdType)) - 1);
	
	//	Activate CS low to start transaction
	gpio_write_io (CSN_n, DATA_LOW);
	cmd_delay (2);
	for (i=0;i<nBits;i++) {
		//	Activate SPI data
		if ((uCmd & 0x80000000) == 0x80000000) {
			gpio_write_io (SDA_n, DATA_HIGH);
		}
		if ((uCmd & 0x80000000) == DATA_LOW) {
			gpio_write_io (SDA_n, DATA_LOW);
		}
		cmd_delay (2);
		//	Toggle SPI clock
		gpio_write_io (SCL_n, DATA_HIGH);
		cmd_delay (2);
		gpio_write_io (SCL_n, DATA_LOW);
		cmd_delay (2);
		uCmd <<= 1;
	}
	//	Pull low data
	gpio_write_io (SDA_n, DATA_LOW);
	//	Activate CS high to stop transaction
	gpio_write_io (CSN_n, DATA_HIGH);
	cmd_delay (2);
}

void TFT_TXDT240C_5182A_Init(void)
{   	
	//	Reset TFT control register at first
   // R_TFT_CTRL=0;
    
	TPO_SPI_WriteCmd1 (0, 0x000D0000);
	cmd_delay (5);
	TPO_SPI_WriteCmd1 (0, 0x00050000);
	cmd_delay (5);
	TPO_SPI_WriteCmd1 (0, 0x00050000);
	cmd_delay (5);
	TPO_SPI_WriteCmd1 (0, 0x000D0000);
	cmd_delay (5);
	//while(1)
	TPO_SPI_WriteCmd1 (0, 0x60010000);
	cmd_delay (5);
	TPO_SPI_WriteCmd1 (0, 0x40030000);
	TPO_SPI_WriteCmd1 (0, 0xA0080000);
	TPO_SPI_WriteCmd1 (0, 0x50680000);
	
	/*
	R_TFT_HS_WIDTH			= 1;				//	1		=HPW
	R_TFT_H_START			= 1+100-1;			//	240		=HPW+HBP
	R_TFT_H_END				= 1+100+480-1;	    //	1520	=HPW+HBP+HDE
	R_TFT_H_PERIOD			= 1+100+480+37-1;	//	1560	=HPW+HBP+HDE+HFP
	R_TFT_VS_WIDTH			= 0;				//	1		=VPW				(DCLK)
	R_TFT_V_START			= 1+13-1;			//	21		=VPW+VBP		(LINE)
	R_TFT_V_END				= 13+240;		    //	261		=VPW+VBP+VDE		(LINE)
	R_TFT_V_PERIOD			= 13+240+9;		    //	262		=VPW+VBP+VDE+VFP	(LINE)
	R_TFT_CTRL           	= 0x830F;
	R_TFT_TS_MISC			&= 0xFFF3FF; */
	
	R_TFT_HS_WIDTH			= 1;				//	1		=HPW
	R_TFT_H_START			= 1+252;			//	240		=HPW+HBP
	R_TFT_H_END				= 1+252+1280;	    //	1520	=HPW+HBP+HDE
	R_TFT_H_PERIOD			= 1+252+1280+28;	//	1560	=HPW+HBP+HDE+HFP
	R_TFT_VS_WIDTH			= 1;				//	1		=VPW				(DCLK)
	
	R_TFT_V_START			= 1+13;				//	21		=VPW+VBP			(LINE)
	R_TFT_V_END				= 1+13+240;		    //	261		=VPW+VBP+VDE		(LINE)
	R_TFT_V_PERIOD			= 1+13+240+9;		//	262		=VPW+VBP+VDE+VFP	(LINE)
	R_TFT_CTRL           	= 0x8319;
	R_TFT_TS_MISC		   &= 0xFFF3FF; 		            
	
//	R_IOB_DRV = 0;	
}

//=== This is for code configuration DON'T REMOVE or MODIFY it ===//
#endif //(defined _DRV_L1_TFT) && (_DRV_L1_TFT == 1)                   //
//================================================================//