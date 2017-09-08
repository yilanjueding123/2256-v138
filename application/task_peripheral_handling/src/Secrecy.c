#include "secrecy.h"
#include "customer.h"
#include "application.h"

#if SECRECY_ENABLE
//============================================================
INT32U Secrecy_Data_Tx[2];
INT32U Secrecy_Data_Rx[2];

volatile INT8U Secrecy_Failure_Flag = 0;
volatile INT32U R_Card_Size = 0;
//============================================================
//加密密钥
//-----------------------------------------
//测试密钥
#if 0
static INT32U Secret_Key[4] = {	0x01234567,
								0x89abcdef,
								0x76543210,
								0xfedcba98};

//-----------------------------------------
#else
//2016-07-01 首版量产
static INT32U Secret_Key[4] = {	0x4A4F5948,
								0x4F4E4553,
								0x54000000,
								0x20160630};
#endif
//============================================================
void Secrecy_IO_Init(void)
{
	gpio_init_io(SECRECY_SDA, GPIO_OUTPUT);				
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_HIGH);
	gpio_write_io(SECRECY_SDA, DATA_HIGH);	

	gpio_init_io(SECRECY_SCL, GPIO_OUTPUT);				
	gpio_set_port_attribute(SECRECY_SCL, ATTRIBUTE_HIGH);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);				
}
//--------------------------------------------------------
void Wake_Up_SecrecyIC(void)
{
	gpio_write_io(SECRECY_SCL, DATA_LOW);	
	OSTimeDly(1);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);	
	OSTimeDly(1);
}
//--------------------------------------------------------
//96M:		5 & 4 
//129.6M:	6 & 5  <131.6KHz>
//129.6M:	7 & 6  <74.6KHz>
#define DELAY_COUNT		8 //74.6KHz
void I2C_Delay (INT16U i) 
{
	INT16U j;

	for (j=0;j<(i<<DELAY_COUNT);j++)
		i=i;
}

void I2C_Delay2 (INT16U i) 
{
	INT16U j;

	for (j=0;j<(i<<(DELAY_COUNT-1));j++)
		i=i;
}
//--------------------------------------------------------
void I2C_Start (void)
{
     gpio_write_io(SECRECY_SCL, DATA_HIGH);	
     I2C_Delay (1);
	 gpio_write_io(SECRECY_SDA, DATA_HIGH);  
	 I2C_Delay (1);
	 gpio_write_io(SECRECY_SDA, DATA_LOW);	 
	 I2C_Delay (4);
}
//--------------------------------------------------------
void I2C_Stop (void)
{
    I2C_Delay (1);
    gpio_write_io(SECRECY_SDA, DATA_LOW);
	I2C_Delay (1);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);	
	I2C_Delay (4);
	gpio_write_io(SECRECY_SDA, DATA_HIGH);	
	I2C_Delay (1);
}
//--------------------------------------------------------
INT8U I2C_Write_Byte (INT8U value)
{
	INT16U i;
	INT8U ack;
	INT16U err_cnt = 0;

	for (i=0;i<8;i++)
	{
		gpio_write_io(SECRECY_SCL, DATA_LOW);				
		I2C_Delay2 (1);
		if (value & 0x80)
			gpio_write_io(SECRECY_SDA, DATA_HIGH);			
		else
			gpio_write_io(SECRECY_SDA, DATA_LOW);			
		I2C_Delay2(1);
		gpio_write_io(SECRECY_SCL, DATA_HIGH);				
		I2C_Delay(1);
		value <<= 1;
	}

	gpio_write_io(SECRECY_SCL, DATA_LOW);
	I2C_Delay2(1);				
	gpio_init_io(SECRECY_SDA, GPIO_INPUT);					
	gpio_write_io(SECRECY_SDA, DATA_HIGH);
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_LOW);
	I2C_Delay(1);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);					
	I2C_Delay(1);
	ack = 1;
	while(gpio_read_io(SECRECY_SDA))
	{
		err_cnt++;
		if(err_cnt>200)
		{
			ack = 0;
			break;
		}
		I2C_Delay2(1);
	}
	err_cnt=0;
	gpio_write_io(SECRECY_SCL, DATA_LOW);	
	I2C_Delay2(1);	
	gpio_init_io(SECRECY_SDA, GPIO_OUTPUT); 				
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_HIGH);
	gpio_write_io(SECRECY_SDA, DATA_LOW);

	return ack;
}
//--------------------------------------------------------
INT8U I2C_Read_Byte (void)
{
	INT16U i;
	INT8U data;

	gpio_init_io(SECRECY_SDA, GPIO_INPUT);				
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_LOW);
	gpio_write_io(SECRECY_SDA, DATA_HIGH);
	data = 0x00;
	for (i=0;i<8;i++)
	{
		I2C_Delay(1);
		gpio_write_io(SECRECY_SCL, DATA_HIGH);			
		I2C_Delay2(1);
		data <<= 1;
		data |=( gpio_read_io(SECRECY_SDA));
		I2C_Delay2(1);
		gpio_write_io(SECRECY_SCL, DATA_LOW);		
	}
	return data;
}
//--------------------------------------------------------
void I2C_Ack(void)
{
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_HIGH);
	gpio_write_io(SECRECY_SDA, DATA_LOW);
	gpio_init_io(SECRECY_SDA, GPIO_OUTPUT); 
	I2C_Delay(1);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);
	I2C_Delay(1);
	gpio_write_io(SECRECY_SCL, DATA_LOW);
	I2C_Delay(4);
}
//--------------------------------------------------------
void I2C_NoAck(void)
{
	gpio_set_port_attribute(SECRECY_SDA, ATTRIBUTE_HIGH);
	gpio_write_io(SECRECY_SDA, DATA_HIGH);
	gpio_init_io(SECRECY_SDA, GPIO_OUTPUT); 
	I2C_Delay(1);
	gpio_write_io(SECRECY_SCL, DATA_HIGH);
	I2C_Delay(1);
	gpio_write_io(SECRECY_SCL, DATA_LOW);
	I2C_Delay(2);
}
//--------------------------------------------------------
INT8U I2C_Write_8_Byte (void) 
{
	INT8U i,j;

	I2C_Start ();								
	if (I2C_Write_Byte (SECRECY_IC_ADDR) == 0)
	{
		I2C_Stop();
		return 0;
	}
	I2C_Delay(2);
	for(i=0; i<2; i++)
	{
		for(j=0; j<4; j++)
		{
			if (I2C_Write_Byte (0xFF&(Secrecy_Data_Tx[i]>>(0+8*j))) == 0)
			{
				I2C_Stop();
				return 0;
			}
			I2C_Delay(2);
		}
	}
	I2C_Stop ();

	return 1;
}
//--------------------------------------------------------
INT8U I2C_Read_8_Byte (void)
{
	INT8U i,j;

	Secrecy_Data_Rx[0] = 0;
	Secrecy_Data_Rx[1] = 0;

	I2C_Start ();								
	if (I2C_Write_Byte (SECRECY_IC_ADDR|0x1) == 0)
	{
		I2C_Stop();
		return 0;
	}
	I2C_Delay(2);
	
	for (i=0; i<2; i++)
	{
		for (j=0; j<4; j++)
		{
			Secrecy_Data_Rx[i] |= ((I2C_Read_Byte()&0xFF) << (0+j*8));
			if ((i == 1)&&(j == 3)) I2C_NoAck();
			else I2C_Ack();
		}
	}  
	
	I2C_Stop ();								

	return 1;
}
//============================================================
#define V0_MASK		0xA136B765
#define V1_MASK		0xB53E8492

void decrypt (INT32U* v, INT32U* k) 
{
    INT32U v0=v[0], v1=v[1], sum=0xC6EF3720, i;  /* set up */
    INT32U delta=0x9e3779b9;                     /* a key schedule constant */
    INT32U k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */

	v0 ^= V0_MASK;
	v1 ^= V1_MASK;

	for (i=0; i<32; i++) 
	{                         /* basic cycle start */
        v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
        v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
        sum -= delta;
    }                                              /* end cycle */
    v[0]=v0; v[1]=v1;
}
//============================================================
void read_card_size_set(INT32U val)
{
	R_Card_Size = val;
}

INT8U Send_Secrecy_Data(void)
{
	INT8S ret;

	Wake_Up_SecrecyIC(); //唤醒IC

	
	Secrecy_Data_Tx[0] = (R_Card_Size>>10); //明码1
	Secrecy_Data_Tx[1] = PRODUCT_DATA; //明码2
	ret = I2C_Write_8_Byte(); //写入加密明码

	return ret;
}
//--------------------------------------------------------
INT8S ReceiveData_And_Deciphering(void)
{
	INT8S ret;

	I2C_Read_8_Byte(); //读出加密数据
	DBG_PRINT("Rx_DATA_1 = 0x%08X\r\n",Secrecy_Data_Rx[0]);
	DBG_PRINT("Rx_DATA_2 = 0x%08X\r\n",Secrecy_Data_Rx[1]);

	decrypt(Secrecy_Data_Rx, Secret_Key); //解密数据
	DBG_PRINT("Deciphering DATA_1 = 0x%8X\r\n",Secrecy_Data_Rx[0]);
	DBG_PRINT("Deciphering DATA_2 = 0x%8X\r\n",Secrecy_Data_Rx[1]);

	//对比加密数据
	ret = gp_strncmp((INT8S*)Secrecy_Data_Tx, (INT8S *)Secrecy_Data_Rx, 8); 
	if (ret == 0)
	{//解密正确
		DBG_PRINT("Deciphering Succeed!!");
	}
	else 
	{//解密失败
		DBG_PRINT("Deciphering Failure!!");
	}

	//Secrecy_Failure_Flag = ret;

	DBG_PRINT("\r\n");
	DBG_PRINT("\r\n");
	return ret;
}
//--------------------------------------------------------
extern void led_red_on(void);
extern void led_red_off(void);
void Secrecy_Function(void)
{
	//OS_CPU_SR cpu_sr;

	INT8S ret;
	INT32U cnt = 0;
	INT8U error_cnt = 20;
	
	//OS_ENTER_CRITICAL();
	while(error_cnt)
	{
		Send_Secrecy_Data();
		OSTimeDly(1);
		ret = ReceiveData_And_Deciphering();
		if (ret == 0) break;
		error_cnt--;
	}
	//OS_EXIT_CRITICAL(); 
	Secrecy_Failure_Flag = ret;

	if (ret != 0) 
	{
		DBG_PRINT("\r\nWait ... \r\n");
		while(0) 
		{
			DBG_PRINT(". ");
			led_red_on();
			OSTimeDly(5);
			led_red_off();
			OSTimeDly(195);
			if (++cnt > 30)
			{
				cnt = 0;
				break;
			}
		}
	}
}
//============================================================
#endif

