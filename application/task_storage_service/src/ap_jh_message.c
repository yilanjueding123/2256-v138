#include "ap_storage_service.h"
#include "ap_state_config.h"
#include "ap_state_handling.h"
#include "fs_driver.h"
#include "avi_encoder_app.h"
#include "stdio.h"


const INT8U  R_CopyrightMSG_Buf[]= "版权声明: 本产品由深圳市乐信兴业科技有限公司设计,版权所有,仿冒必究"; //
const INT8U   sensor_type_name[9][13]={"OV7670","OV9712","SOI_H22","BF3703","SOI_H22_MIPI","OV3640","OV5642",
	                                   "GC1004","GC1004_MIPI"
};

static void save_COPYRIGHT_MESSAGE_to_disk(void)
{
    INT16S fd;
	INT32U addr;
	fd = open("C:\\CopyrightMSG.txt", O_RDWR|O_TRUNC|O_CREAT);
	if(fd < 0)
		return;
	addr = (INT32U)gp_malloc(66+1);
	if(!addr)
	{
		close(fd);
		return;
	}
	gp_strcpy((INT8S*)addr, (INT8S *)R_CopyrightMSG_Buf);
	write(fd, addr, 66);
	close(fd);
	gp_free((void*) addr);
}
static void save_VERSION_NUMBER_to_disk(void)
{
    INT8U  *p;
	INT16S fd;
	INT32U addr;
	INT32U product_num;
	INT32U data_num;
	product_num = PRODUCT_NUM;
	data_num = PRODUCT_DATA;
	fd = open("C:\\Version.txt", O_RDWR|O_TRUNC|O_CREAT);
	if(fd < 0)
		return;
	addr = (INT32U)gp_malloc(22+5);
	if(!addr)
	{
		close(fd);
		return;
	}
	p = (INT8U*)addr;
	sprintf((char *)p, (const char *)"%08d,JH%04d,v0.00,", data_num, product_num);
	p[17] = ((PROGRAM_VERSION_NUM%1000)/100) + '0';
	p[19] = ((PROGRAM_VERSION_NUM%100)/10) + '0';
	p[20] = ((PROGRAM_VERSION_NUM%10)/1) + '0';
	write(fd, addr, 22);
	close(fd);
	gp_free((void*) addr);
}

void JH_Message_Get(void)
{
	INT32U addr;
	INT32S nRet;
	INT16S fd;

	fd = open("C:\\jh_message.txt", O_RDWR);
	
	if(fd >= 0)
		{
		   addr = (INT32U)gp_malloc(20+5);
		   if(!addr)
		   	goto Fail_Return_2;
		   nRet = read(fd, addr, 20);
		   if(nRet <= 0)
		   	goto Fail_Return;
		   
		}
	else
		return;
	
	nRet = gp_strncmp((INT8S*)addr, (INT8S *)"COPYFIGHT MESSAGE? ", 19);
	if (nRet==0) { save_COPYRIGHT_MESSAGE_to_disk(); goto Fail_Return; }
	nRet = gp_strncmp((INT8S*)addr, (INT8S *)"VERSION_NUMBER??", 16);
	if (nRet==0) { save_VERSION_NUMBER_to_disk(); goto Fail_Return; }
	
Fail_Return:
	gp_free((void*) addr);
Fail_Return_2:
	if(fd>=0) close(fd);
}
