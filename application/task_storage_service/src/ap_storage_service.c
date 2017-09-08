#include "ap_storage_service.h"
#include "string.h"
#include "secrecy.h"

#define USE_LONG_FILE_NAME		0
#define AVI_REC_MAX_BYTE_SIZE   0x70000000  //1879048192 Bytes//78643200//4B00000
#define AP_STG_MAX_FILE_NUMS    625
#define BKGROUND_DETECT_INTERVAL  (5*128)  // 20 second
#define BKGROUND_DEL_INTERVAL     (1*128)   // 2 second

static INT32S g_jpeg_index;
static INT32S g_avi_index;
static INT32S g_wav_index;
static INT32S g_file_index;
static INT16U g_file_num;
static INT16S g_latest_avi_file_index;
static INT32U g_avi_file_time;
static INT32U g_jpg_file_time;
static INT8U g_avi_index_9999_exist;
static INT32U g_avi_file_oldest_time;
static INT16S g_oldest_avi_file_index;
static CHAR g_file_path[32];
#if USE_LONG_FILE_NAME
static CHAR g_file_path2[50];
#endif
static INT8U sd_upgrade_file_flag = 0;
static INT8U curr_storage_id;
static INT8U storage_mount_timerid;
//static INT8U err_handle_timerid;
#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
static CHAR g_next_file_path[32];
#endif

INT32U gSelect_Size;
INT32U gOutOfFreeSpace;

static INT16U avi_file_table[625];
static INT16U jpg_file_table[625];
static INT16U wav_file_table[625];
static INT8U device_plug_phase=0;

#if C_AUTO_DEL_FILE == CUSTOM_ON
static INT8U storage_freesize_timerid;
static INT8U ap_step_work_start=0;
static INT32U BkDelThreadMB;

static INT32S retry_del_idx=-1;
static INT8U retry_del_counts=0;
#endif

static INT8S bkground_del_disble=0;

//	prototypes
void ap_storage_service_error_handle(INT8U type);
INT32S get_file_final_avi_index(void);
INT32S get_file_final_jpeg_index(void);
INT32S get_file_final_wav_index(void);
INT32S rtc_time_get_and_start(void);
static INT8U card_first_dete=1;
extern INT8U card_space_less_flag;



//Huajun add @ 2013-06-24

#if USE_LONG_FILE_NAME

#define   DataPot		20							//spot id place
#define   MaxSpot 		4							//spot id num
static INT8U PosSpot[MaxSpot] = {4,7,13,16};
static INT8S SelectFileName(INT8S *filename)
{
	INT8U i;
	for(i=0; i<MaxSpot; i++)
	{
		if(filename[PosSpot[i]] != '-')
		{
			return -1;
		}
	}
	
	return 0;
} 

static INT8S PollFilePos(f_pos *fpos,INT8S *ext,INT16U ID)
{
	INT16S nRet;
	struct f_info   file_info;
	INT8S  filename[30]="?\?\?\?-?\?-?\? ?\?-?\?-?\?";

	snprintf( (void *)&filename[19], 10, "(%04d).", ID);			//Safer than sprintf
	memcpy(&filename[26],ext,3);
	filename[29] = '\0';
	
	
	nRet = _findfirst((void *)filename, &file_info, D_ALL);	

	if(nRet == 0)
	{
		get_fnode_pos(fpos);
	}

	return nRet;
}


static INT8S CreatFileName(INT32U fimename, INT16U index)
{
	TIME_T timeStr;

	cal_time_get(&timeStr);
	snprintf( (void *)fimename, 30, "%04d-%02d-%02d %02d-%02d-%02d(%04d)", timeStr.tm_year,timeStr.tm_mon,timeStr.tm_mday,timeStr.tm_hour,timeStr.tm_min,timeStr.tm_sec,index);			
	
	return 0;
}

#endif

//end


void ap_storage_service_init(void)
{	

#if C_AUTO_DEL_FILE == CUSTOM_ON
	storage_freesize_timerid = 0xFF;
#endif
	DBG_PRINT("mmmmm\r\n");

	storage_mount_timerid = STORAGE_SERVICE_MOUNT_TIMER_ID;
	sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_STORAGE_CHECK, storage_mount_timerid, STORAGE_TIME_INTERVAL_MOUNT);

}

void bkground_del_disable(INT32U disable1_enable0)
{
    bkground_del_disble=disable1_enable0;
}

INT8S bkground_del_disable_status_get(void)
{
    return bkground_del_disble;
}

INT8U storage_sd_upgrade_file_flag_get(void)
{
	return sd_upgrade_file_flag;
}

void ap_storage_service_error_handle(INT8U type)
{
	INT32U led_type;
				
	led_type = LED_WAITING_CAPTURE;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_FLASH_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
}

#if C_AUTO_DEL_FILE == CUSTOM_ON
void ap_storage_service_freesize_check_switch(INT8U type)
{
	if (type == TRUE) {
		if (storage_freesize_timerid == 0xFF) {
			storage_freesize_timerid = STORAGE_SERVICE_FREESIZE_TIMER_ID;
			sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, STORAGE_TIME_INTERVAL_FREESIZE);
		}
	} else {
		if (storage_freesize_timerid != 0xFF) {
			sys_kill_timer(storage_freesize_timerid);
			storage_freesize_timerid = 0xFF;
		}
	}
}

INT32S ap_storage_service_freesize_check_and_del(void)
{
#if USE_LONG_FILE_NAME
	struct f_info finfo;
	f_pos fpos;
#else
	struct stat_t buf_tmp;
	CHAR f_name[50];
#endif
	INT32U i, j;
	INT32S del_index, ret;
	INT64U  disk_free_size;
    INT32S  step_ret;   
    INT16S del_ret;
	
    ret = STATUS_OK;

    if(ap_step_work_start==0)
    {
        disk_free_size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE) >> 20;
//        ap_storage_service_del_thread_mb_set();
         BkDelThreadMB=AUTO_DEL_THREAD_MB;
RE_DEL:
        DBG_PRINT("\r\n[Bkgnd Del Detect (DskFree: %d MB)]\r\n",disk_free_size);
      //  DBG_PRINT("[Thread:%dMB]\r\n",BkDelThreadMB);

        if (bkground_del_disble==1)  /*闽超璉春郎璶暗ㄆ*/
        {
            if (disk_free_size<50)
            {
                sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, 128);
                // if sdc redundant size less than 3 MB, STOP recording now.
                if(disk_free_size<10) {
                   // sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, 64);
                  //  if (disk_free_size<=3) {
                        AviPacker_Break_Set(1);
                        msgQSend(ApQ, MSG_APQ_VDO_REC_STOP, NULL, NULL, MSG_PRI_NORMAL);
                        sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, BKGROUND_DETECT_INTERVAL);
                  //  }
                }
            }
            return 0;
        }

    	if (disk_free_size <= BkDelThreadMB)
        {
            if(g_avi_index_9999_exist)
            {
			    del_index = g_oldest_avi_file_index;

			  #if USE_LONG_FILE_NAME
				PollFilePos(&fpos,(void*)"AVI",g_oldest_avi_file_index);
			  #else
			    sprintf((char *)f_name, (const char *)"MOVI%04d.avi", g_oldest_avi_file_index);
			  #endif
		    }
            else
            {
    		    del_index = -1;
    		    for (i=0 ; i<AP_STG_MAX_FILE_NUMS ; i++)
                {
        			if (avi_file_table[i]) {
        				for (j=0 ; j<16 ; j++) {
        					if (avi_file_table[i] & (1<<j)) {
        						del_index = (i << 4) + j;

							  #if USE_LONG_FILE_NAME
        						PollFilePos(&fpos,(void *)"AVI",del_index);
        						GetFileInfo(&fpos,&finfo);
        						if(finfo.f_attrib == D_RDONLY){
        					  #else
        						sprintf((char *)f_name, (const char *)"MOVI%04d.avi", del_index);
        						stat(f_name, &buf_tmp);
        						if((buf_tmp.st_mode & D_RDONLY) != 0) {
        					  #endif
        							del_index = -1;
        						}else{
	        						break;
	        					}
        					}
        				}
        				if (del_index != -1) {
        					break;
        				}
        			}
    		    }
            }
            
#if USE_LONG_FILE_NAME
            if (del_index == -1 || gp_strcmp((INT8S*)finfo.f_name,(INT8S*)g_file_path) == 0) {
#else
            if (del_index == -1 || gp_strcmp((INT8S*)f_name,(INT8S*)g_file_path) == 0) {
#endif
        		if (disk_free_size <= 50) {
        		    msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, NULL, NULL, MSG_PRI_NORMAL);
        			bkground_del_disble = 1;
        		} 
                return STATUS_FAIL;
            }
#if USE_LONG_FILE_NAME
    		DBG_PRINT("\r\nDel <%s>\r\n", finfo.f_name);
#else
    		DBG_PRINT("\r\nDel <%s>\r\n", f_name);
#endif
            //chdir("C:\\DCIM");
            unlink_step_start();
#if USE_LONG_FILE_NAME
            del_ret = sfn_unlink(&fpos);
#else
            del_ret = unlink(f_name);
#endif
    		if (del_ret< 0)
            {
                if (retry_del_idx<0) {
                    retry_del_idx = del_index;
                } else {
                    retry_del_counts++;
                }

                if (retry_del_counts>2)
                {
                    retry_del_idx = -1;  // reset retry index
                    retry_del_counts = 0;  // reset retry counts
    			    avi_file_table[del_index >> 4] &= ~(1 << (del_index & 0xF));
    			    g_file_num--;
                    DBG_PRINT("Del Fail, avoid\r\n");
                } else {
                    DBG_PRINT("Del Fail, retry\r\n");
                }

    			ret = STATUS_FAIL;
                sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, 128);
    		}
            else
            {
    		    retry_del_idx = -1;  // reset retry index
    		    retry_del_counts = 0;  // reset retry counts
    		    ap_step_work_start=1;
                sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, BKGROUND_DEL_INTERVAL);
    			avi_file_table[del_index >> 4] &= ~(1 << (del_index & 0xF));
    			g_file_num--;
    			DBG_PRINT("Step Del Init OK\r\n");

                if(g_avi_index_9999_exist)
                {
			        get_file_final_avi_index();
			        get_file_final_jpeg_index();
			        get_file_final_wav_index();
                }
		    }

            if (disk_free_size<=70ULL)
            {
               // sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, BKGROUND_DETECT_INTERVAL);
                unlink_step_flush();
                ap_step_work_start=0;
                //
                if ((vfsFreeSpace(MINI_DVR_STORAGE_TYPE) >> 20) <70ULL)
                {
                    DBG_PRINT ("Re-Delete\r\n");
                    goto RE_DEL;
                }
            }
            ret = STATUS_OK;
        }
    }
    else
    {
        step_ret = unlink_step_work();
        if(step_ret!=0)
        {
            sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, BKGROUND_DEL_INTERVAL);
            DBG_PRINT ("StepDel Continue\r\n");
        } else {
            ap_step_work_start=0;
            DBG_PRINT ("StepDel Done\r\n");

            disk_free_size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE) >> 20;
            DBG_PRINT("\r\n[Dominant (DskFree: %d MB)]\r\n", disk_free_size);
            if (disk_free_size > BkDelThreadMB)
            {
                sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, BKGROUND_DETECT_INTERVAL);
            } else {
                // Ч皑弧ぃ镑, 笿郎, 硉盎代 1 second scan
                sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, 128);
            }
        }
    }
	return ret;
}
#endif

void ap_storage_service_timer_start(void)
{
	if (storage_mount_timerid == 0xFF) {
		storage_mount_timerid = STORAGE_SERVICE_MOUNT_TIMER_ID;
		sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_STORAGE_CHECK, storage_mount_timerid, STORAGE_TIME_INTERVAL_MOUNT);
	}
}

void ap_storage_service_timer_stop(void)
{
//DBG_PRINT("gggg\r\n");
	curr_storage_id = 0x55;
	if (storage_mount_timerid != 0xFF) {
		sys_kill_timer(storage_mount_timerid);
		storage_mount_timerid = 0xFF;
	}
}

void ap_storage_service_usb_plug_in(void)
{
	device_plug_phase = 0;
}

extern void read_card_size_set(INT32U val);
INT32S ap_storage_service_storage_mount(void)
{
	INT32S nRet, i;
	INT32U size;
	INT32S led_type = -1;
	static INT8U sd_status_flag = 1;
		if(storage_mount_timerid == 0xff) return	STATUS_FAIL;
	if (device_plug_phase==0)
    {
      ap_storage_service_timer_stop();	
        gOutOfFreeSpace = 0;
        gSelect_Size = 5*1024*1024/512;     // select 5M free size
        nRet = _devicemount(MINI_DVR_STORAGE_TYPE);
		if(FatSpeedUp_Array[MINI_DVR_STORAGE_TYPE].UsedCnt == 0)
		{   
		    gSelect_Size = 500*1024/512;    // select 500K free size
		    nRet = _devicemount(MINI_DVR_STORAGE_TYPE);
			if(FatSpeedUp_Array[MINI_DVR_STORAGE_TYPE].UsedCnt == 0)
				gOutOfFreeSpace = 1;
		}
    }
    else 
    {
        nRet = drvl2_sdc_live_response();
    }

    if (nRet!=0)
    {
        device_plug_phase = 0;  // plug out phase
        ap_storage_service_timer_stop();
        nRet = _devicemount(MINI_DVR_STORAGE_TYPE);  
        if (nRet==0) {
            curr_storage_id = NO_STORAGE;
			DBG_PRINT("jh_dbg_0506_10\r\n");
            //DBG_PRINT ("Retry OK\r\n");
            device_plug_phase = 1;  // plug in phase
            sd_status_flag = 2;
        }
    } else {
        device_plug_phase = 1;  // plug in phase
    }        

	//if (sd_status_flag)
	//{
	//	if (sd_status_flag == 1)
	//		led_type = LED_CARD_DETE_SUC;
	//	else 	
	//		led_type = LED_WAITING_CAPTURE;
	//	sd_status_flag  =0;	
	//}

	if (nRet < 0) {
		card_first_dete=2;
		if (curr_storage_id != NO_STORAGE) {
			DBG_PRINT("jh-p\r\n");
			//ap_storage_service_error_handle(TRUE);
			curr_storage_id = NO_STORAGE;
//			OSTaskChangePrio(STORAGE_SERVICE_PRIORITY, STORAGE_SERVICE_PRIORITY2);
			msgQSend(ApQ, MSG_STORAGE_SERVICE_NO_STORAGE, &curr_storage_id, sizeof(INT8U), MSG_PRI_NORMAL);
		}
		card_space_less_flag =0;
		led_type = LED_NO_SDC;
	    msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		ap_storage_service_timer_start();
		return STATUS_FAIL;
	} else {
	  if(card_first_dete)
				 {
				 card_space_less_flag =0;
				  if(card_first_dete ==1)
				  led_type=LED_CARD_DETE_SUC;
				  else
				  led_type=LED_WAITING_RECORD;	 
				  card_first_dete=0;
				  //msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
				 }
		if (curr_storage_id != MINI_DVR_STORAGE_TYPE) {
//			ap_storage_service_error_handle(FALSE);
			size = vfsFreeSpace(MINI_DVR_STORAGE_TYPE)>>20;
			#if SECRECY_ENABLE
			read_card_size_set(size); //将TF卡容量赋值给加密IC, 做明码使用
			#endif
			DBG_PRINT("Mount OK, free size [%d]\r\n", size);
			if (size < REMAIN_SPACE) {
				led_type = LED_CARD_NO_SPACE;
	           // msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
				//ap_storage_service_error_handle(TRUE);
			}
			else
				card_space_less_flag =0;
			curr_storage_id = MINI_DVR_STORAGE_TYPE;
//			OSTaskChangePrio(STORAGE_SERVICE_PRIORITY2, STORAGE_SERVICE_PRIORITY);
			//rtc_time_get_and_start();
			//mkdir("C:\\DCIM");
			//chdir("C:\\DCIM");
			mkdir("C:\\VIDEO");
			mkdir("C:\\PHOTO");
			g_jpeg_index = g_avi_index = g_wav_index = g_file_index = 0;
//			g_play_index = -1;
			for (i=0 ; i<625 ; i++) {
				avi_file_table[i] = jpg_file_table[i] = wav_file_table[i] = 0;
			}
			chdir("C:\\VIDEO");
			get_file_final_avi_index();
			chdir("C:\\PHOTO");
			get_file_final_jpeg_index();
			chdir("C:\\VIDEO");
			//get_file_final_wav_index();
			if (g_avi_index > g_jpeg_index) {
				if (g_avi_index > g_wav_index) {
					g_file_index = g_avi_index;
				} else {
					g_file_index = g_wav_index;
				}
			} else {
				if (g_jpeg_index > g_wav_index) {
					g_file_index = g_jpeg_index;
				} else {
					g_file_index = g_wav_index;
				}
			}
			//JH_Message_Get();
			if (sd_upgrade_file_flag == 0) 
				{
                    INT8U  FileName_Path[3+5+8+4+1];				
					struct f_info	file_info;
					char   p[4];
					chdir("C:\\");
				
                     JH_Message_Get();
					 sprintf((char*)p,(const char*)"%04d",PRODUCT_NUM);
	                 gp_strcpy((INT8S*)FileName_Path,(INT8S*)"C:\\JH_");
	                 gp_strcat((INT8S*)FileName_Path,(INT8S*)p);
	                 gp_strcat((INT8S*)FileName_Path,(INT8S*)"*.bin");

	                 nRet = _findfirst((CHAR*)FileName_Path, &file_info ,D_ALL); //查找bin文件
	                 if (nRet < 0) 
	                  {
		              sd_upgrade_file_flag =1;
					  #if SECRECY_ENABLE
		              msgQSend(StorageServiceQ, MSG_STORAGE_SERVICE_SECU_READ, NULL, NULL, MSG_PRI_NORMAL);//加密计算
					  #endif
					  DBG_PRINT("No updatafile\r\n");
	                  }
					 else
					 {
					  sd_upgrade_file_flag = 2;
					  DBG_PRINT("updatafile\r\n");
					  
					 }

				
				 }
			msgQSend(ApQ, MSG_STORAGE_SERVICE_MOUNT, &curr_storage_id, sizeof(INT8U), MSG_PRI_NORMAL);
		}
		ap_storage_service_timer_start();
		if (led_type != -1)	
		msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &led_type, sizeof(INT32U), MSG_PRI_NORMAL);
		
		return STATUS_OK;
	}
}

void ap_storage_service_file_open_handle(INT32U req_type)
{
	INT32U reply_type;
	STOR_SERV_FILEINFO file_info;

	INT32U del_index;
#if C_AUTO_DEL_FILE == CUSTOM_ON
	INT32U i,j;
#endif

#if USE_LONG_FILE_NAME
	struct f_info finfo;
	f_pos fpos;
#else
#if C_AUTO_DEL_FILE == CUSTOM_ON
	struct stat_t buf_tmp;
	CHAR f_name[50];
#endif
#endif

	if (storage_mount_timerid != 0xFF) {
		sys_kill_timer(storage_mount_timerid);
		storage_mount_timerid = 0xFF;
	}
	file_info.storage_free_size = (vfsFreeSpace(curr_storage_id) >> 20);
#if 0
#if USE_LONG_FILE_NAME

	g_file_path[0] = 0;

#else

	while ( (avi_file_table[g_file_index >> 4] & (1 << (g_file_index & 0xF))) ||
			(jpg_file_table[g_file_index >> 4] & (1 << (g_file_index & 0xF))) ||
			(wav_file_table[g_file_index >> 4] & (1 << (g_file_index & 0xF))) )
	{
		g_file_index++;
		if(g_file_index >= 10000) {
			g_file_index = 0;
		}
	}

#endif
#endif
    switch (req_type)
    {
    	case MSG_STORAGE_SERVICE_VID_REQ:
			reply_type = MSG_STORAGE_SERVICE_VID_REPLY;

	

		//	sprintf((char *)g_file_path, (const char *)"MOVI%04d.avi", g_file_index);

		
		    sprintf((char *)g_file_path, (const char *)"C:\\VIDEO\\MOVI%04d.avi", g_avi_index);

			del_index = 0;

		#if C_AUTO_DEL_FILE == CUSTOM_ON
			if (file_info.storage_free_size <= 5) {
			    del_index = -1;
    		    for (i=0 ; i<AP_STG_MAX_FILE_NUMS ; i++)
                {
        			if (avi_file_table[i]) {
        				for (j=0 ; j<16 ; j++) {
        					if (avi_file_table[i] & (1<<j)) {
        						del_index = (i << 4) + j;

							  #if USE_LONG_FILE_NAME

  								if(PollFilePos(&fpos, (void*)"AVI",del_index) == 0)
  								{
									GetFileInfo(&fpos, &finfo); 
									memcpy(g_file_path,finfo.f_name,30);		
								     						
	        						if(finfo.f_attrib == D_RDONLY){
	        							del_index = -1;
	        						}else{
		        						break;
		        					}
	        					}

	        				  #else

        						sprintf((char *)f_name, (const char *)"MOVI%04d.avi", del_index);
        						stat(f_name, &buf_tmp);
								if((buf_tmp.st_mode & D_RDONLY) != 0)
								{
									del_index = -1;
								} else {
									break;
								}
								
	        				  #endif
        					}
        				}
        				if (del_index != -1) {
        					break;
        				}
        			}
    		    }
    		}
		#endif    		
    	
    		if (del_index == -1) {
    		    file_info.file_handle = -2;
    		} else {   		

			  

			    file_info.file_handle = open (g_file_path, O_WRONLY|O_CREAT|O_TRUNC);
    		}
    		
    		if (file_info.file_handle >= 0)
            {
				file_info.file_path_addr = (INT32U) g_file_path;
				if( (avi_file_table[g_avi_index >> 4] & (1 << (g_avi_index & 0xF))) == 0) {
					avi_file_table[g_avi_index >> 4] |= 1 << (g_avi_index & 0xF);
					//g_file_num++;
				}
				g_avi_index++;
				if(g_avi_index == 10000){
					g_avi_index = 0;
				}				
				//g_avi_index = g_file_index;
				DBG_PRINT("FileName = %s\r\n", file_info.file_path_addr);
				//write(file_info.file_handle, 0, 1);
				//lseek(file_info.file_handle, 0, SEEK_SET);

			}
            break;

        case MSG_STORAGE_SERVICE_PIC_REQ:
			reply_type = MSG_STORAGE_SERVICE_PIC_REPLY;

		
		  
			//sprintf((char *)g_file_path, (const char *)"PICT%04d.jpg", g_file_index);		

		  
			sprintf((char *)g_file_path, (const char *)"C:\\PHOTO\\PICT%04d.jpg", g_jpeg_index);

    		file_info.file_handle = open (g_file_path, O_WRONLY|O_CREAT|O_TRUNC);

			if (file_info.file_handle >= 0) {
				file_info.file_path_addr = (INT32U) g_file_path;
				if( (jpg_file_table[g_jpeg_index >> 4] & (1 << (g_jpeg_index & 0xF))) == 0){
					jpg_file_table[g_jpeg_index >> 4] |= 1 << (g_jpeg_index & 0xF);
					//g_file_num++;
				}
				g_jpeg_index++;
				if(g_jpeg_index == 10000){
					g_jpeg_index = 0;
				}				
				//g_jpeg_index = g_file_index;
				DBG_PRINT("FileName = %s\r\n", file_info.file_path_addr);
			}
            break;

    	case MSG_STORAGE_SERVICE_AUD_REQ:
    		reply_type = MSG_STORAGE_SERVICE_AUD_REPLY;

#if USE_LONG_FILE_NAME

		#if AUD_REC_FORMAT == AUD_FORMAT_WAV
			if(PollFilePos(&fpos,(void*)"WAV",g_file_index) == 0)
			{
				sfn_unlink(&fpos);							
			}
			CreatFileName((INT32U)g_file_path, g_file_index);
	    	memcpy((INT8S *)g_file_path+strlen(g_file_path),".WAV\0",5);
 
		#else
			if(PollFilePos(&fpos,(void*)"MP3",g_file_index) == 0)
			{
				sfn_unlink(&fpos);							
			}	
		    CreatFileName((INT32U)g_file_path, g_file_index);
	    	memcpy((INT8S *)g_file_path+strlen(g_file_path),".MP3\0",5);

		#endif

#else

		#if AUD_REC_FORMAT == AUD_FORMAT_WAV
			sprintf((char *)g_file_path, (const char *)"RECR%04d.wav", g_file_index);
		#else
			sprintf((char *)g_file_path, (const char *)"RECR%04d.MP3", g_file_index);
		#endif

#endif

    		file_info.file_handle = open (g_file_path, O_WRONLY|O_CREAT|O_TRUNC);
    		if (file_info.file_handle >= 0) {
    			file_info.file_path_addr = (INT32U) g_file_path;
    			if( (wav_file_table[g_file_index >> 4] & (1 << (g_file_index & 0xF))) == 0){
    				wav_file_table[g_file_index >> 4] |= 1 << (g_file_index & 0xF);
    				g_file_num++;
    			}
    			g_file_index++;
				if(g_file_index == 10000){
					g_file_index = 0;
				}    			
    			g_wav_index = g_file_index;
    			DBG_PRINT("FileName = %s\r\n", file_info.file_path_addr);
			}
            break;

        default:
            DBG_PRINT ("UNKNOW STORAGE SERVICE\r\n");
            break;
    }

	msgQSend(ApQ, reply_type, &file_info, sizeof(STOR_SERV_FILEINFO), MSG_PRI_NORMAL);
}

#if C_CYCLIC_VIDEO_RECORD == CUSTOM_ON
void ap_storage_service_cyclic_record_file_open_handle(void)
{
	INT32U reply_type;
	STOR_SERV_FILEINFO file_info;

#if USE_LONG_FILE_NAME
	struct f_info finfo;
	f_pos fpos;
	//struct stat_t buf_tmp;
#endif

	if (storage_mount_timerid != 0xFF) {
		sys_kill_timer(storage_mount_timerid);
		storage_mount_timerid = 0xFF;
	}

	reply_type = MSG_STORAGE_SERVICE_VID_CYCLIC_REPLY;


	//sprintf((char *)g_next_file_path, (const char *)"MOVI%04d.avi", g_file_index);
	
	sprintf((char *)g_next_file_path, (const char *)"C:\\VIDEO\\MOVI%04d.avi", g_avi_index);
	while (avi_file_table[g_avi_index >> 4] & (1 << (g_avi_index & 0xF)))
	{
		g_avi_index++;
		if(g_avi_index >= 10000) {
			g_avi_index = 0;
		}
		//sprintf((char *)g_file_path, (const char *)"MOVI%04d.avi", g_file_index);
		
		sprintf((char *)g_next_file_path, (const char *)"C:\\VIDEO\\MOVI%04d.avi", g_avi_index);
	}



	file_info.file_handle = open (g_next_file_path, O_WRONLY|O_CREAT|O_TRUNC);
	if (file_info.file_handle >= 0) {
		file_info.file_path_addr = (INT32U) g_next_file_path;
		if( (avi_file_table[g_avi_index >> 4] & (1 << (g_avi_index & 0xF))) == 0){
			avi_file_table[g_avi_index >> 4] |= 1 << (g_avi_index & 0xF);
			//g_file_num++;
		}
		g_avi_index++;
		if(g_avi_index == 10000){
			g_avi_index = 0;
		}			
		///g_avi_index = g_file_index;
	}
	DBG_PRINT("FileName = %s\r\n", file_info.file_path_addr);
	msgQSend(ApQ, reply_type, &file_info, sizeof(STOR_SERV_FILEINFO), MSG_PRI_NORMAL);

#if C_AUTO_DEL_FILE == CUSTOM_ON
	if (storage_freesize_timerid == 0xFF) {
		storage_freesize_timerid = STORAGE_SERVICE_FREESIZE_TIMER_ID;
		sys_set_timer((void*)msgQSend, (void*)StorageServiceQ, MSG_STORAGE_SERVICE_FREESIZE_CHECK, storage_freesize_timerid, STORAGE_TIME_INTERVAL_FREESIZE);
	}
#endif
}
#endif

INT32S get_file_final_avi_index(void)
{
	CHAR  *pdata;
	INT32U temp_time;
	struct f_info file_info;
	INT32S nRet, temp;
	struct stat_t buf_tmp;
	
	g_avi_index = -1;
	g_avi_index_9999_exist = g_avi_file_time = g_avi_file_oldest_time = 0;
	nRet = _findfirst("*.avi", &file_info, D_ALL);
	if (nRet < 0) {
		g_avi_index++;
		return g_avi_index;
	}
	while (1) {	
		pdata = (CHAR *) file_info.f_name;
		
		// Remove 0KB AVI files
		if (file_info.f_size<512 && unlink((CHAR *) file_info.f_name)==0) {
			nRet = _findnext(&file_info);
			if (nRet < 0) {
				break;
			}
			continue;
		}

        //stat((CHAR *) file_info.f_name, &buf_tmp);

#if USE_LONG_FILE_NAME
		if (SelectFileName((INT8S *) pdata) == 0) {
			temp = (*(pdata + DataPot) - 0x30)*1000;
			temp += (*(pdata + DataPot+1) - 0x30)*100;
			temp += (*(pdata + DataPot+2) - 0x30)*10;
			temp += (*(pdata + DataPot+3) - 0x30);
#else
		if (gp_strncmp((INT8S *) pdata, (INT8S *) "MOVI", 4) == 0) {
			temp = (*(pdata + 4) - 0x30)*1000;
			temp += (*(pdata + 5) - 0x30)*100;
			temp += (*(pdata + 6) - 0x30)*10;
			temp += (*(pdata + 7) - 0x30);
#endif
			if (temp < 10000) {
				avi_file_table[temp >> 4] |= 1 << (temp & 0xF);
			//	if(count_total_num_enable){
			//		g_file_num++;
			//	}
				if (temp > g_avi_index) {
					g_avi_index = temp;
				}
				temp_time = (file_info.f_date<<16)|file_info.f_time;
				if( ((!g_avi_file_time) || (temp_time > g_avi_file_time)) && (buf_tmp.st_mode == D_NORMAL) ){
					g_avi_file_time = temp_time;
					g_latest_avi_file_index = temp;
				}
				if( (!g_avi_file_oldest_time) || (temp_time < g_avi_file_oldest_time) ){
					g_avi_file_oldest_time = temp_time;
					g_oldest_avi_file_index = temp;
				}
			}
		}
		nRet = _findnext(&file_info);
		if (nRet < 0) {
			break;
		}
	}
	g_avi_index++;
	if(g_avi_index == 10000){
		g_avi_index_9999_exist = 1;
		g_avi_index = g_latest_avi_file_index+1;
	}

	return g_avi_index;
}

INT32S get_file_final_jpeg_index(void)
{
	CHAR  *pdata;
	struct f_info   file_info;
	INT16S latest_file_index;
	INT32S nRet, temp;
	INT32U temp_time;

	g_jpeg_index = -1;
	g_jpg_file_time = 0;

	nRet = _findfirst("*.jpg", &file_info, D_ALL);
	if (nRet < 0) {
		g_jpeg_index++;
		return g_jpeg_index;

	}
	while (1) {
		pdata = (CHAR*)file_info.f_name;

		// Remove 0KB JPG files
		if (file_info.f_size<256 && unlink((CHAR *) file_info.f_name)==0) {
			nRet = _findnext(&file_info);
			if (nRet < 0) {
				break;
			}
			continue;
		}

#if USE_LONG_FILE_NAME
		if (SelectFileName((INT8S *) pdata) == 0) {
			temp = (*(pdata + DataPot) - 0x30)*1000;
			temp += (*(pdata + DataPot+1) - 0x30)*100;
			temp += (*(pdata + DataPot+2) - 0x30)*10;
			temp += (*(pdata + DataPot+3) - 0x30);
#else
		if (gp_strncmp((INT8S *) pdata, (INT8S *) "PICT", 4) == 0) {
			temp = (*(pdata + 4) - 0x30)*1000;
			temp += (*(pdata + 5) - 0x30)*100;
			temp += (*(pdata + 6) - 0x30)*10;
			temp += (*(pdata + 7) - 0x30);
#endif
			if (temp < 10000) {
				jpg_file_table[temp >> 4] |= 1 << (temp & 0xF);
				//if(count_total_num_enable){
				//	g_file_num++;
				//}
				if (temp > g_jpeg_index) {
					g_jpeg_index = temp;
				}

				temp_time = (file_info.f_date<<16)|file_info.f_time;
				if( (!g_jpg_file_time) || (temp_time > g_jpg_file_time) ){
					g_jpg_file_time = temp_time;
					latest_file_index = temp;
				}
			}
		}
		nRet = _findnext(&file_info);
		if (nRet < 0) {
			break;
		}
	}
	g_jpeg_index++;
	if( (g_jpeg_index == 10000) || (g_avi_index_9999_exist == 1) ){
		g_avi_index_9999_exist = 1;
		g_jpeg_index = latest_file_index+1;
		g_avi_index = g_latest_avi_file_index+1;
	}

	return g_jpeg_index;
}

INT32S get_file_final_wav_index(void)
{
	CHAR  *pdata;
	struct f_info   file_info;
	INT32S nRet, temp;
	
	g_wav_index = -1;	
#if AUD_REC_FORMAT == AUD_FORMAT_WAV
	nRet = _findfirst("*.wav", &file_info, D_ALL);
#else
	nRet = _findfirst("*.mp3", &file_info, D_ALL);
#endif
	if (nRet < 0) {
		g_wav_index++;
		return g_wav_index;
		
	}	
	while (1) {	
		pdata = (CHAR*)file_info.f_name;

#if USE_LONG_FILE_NAME
		if (SelectFileName((INT8S *) pdata) == 0) {
			temp = (*(pdata + 4) - DataPot)*1000;
			temp += (*(pdata + 5) - DataPot+1)*100;
			temp += (*(pdata + 6) - DataPot+2)*10;
			temp += (*(pdata + 7) - DataPot+3);
#else
		if (gp_strncmp((INT8S *) pdata, (INT8S *) "RECR", 4) == 0) {
			temp = (*(pdata + 4) - 0x30)*1000;
			temp += (*(pdata + 5) - 0x30)*100;
			temp += (*(pdata + 6) - 0x30)*10;
			temp += (*(pdata + 7) - 0x30);
#endif

			wav_file_table[temp >> 4] |= 1 << (temp & 0xF);
			if (temp > g_wav_index) {
				g_wav_index = temp;
			}
		}

		nRet = _findnext(&file_info);
		if (nRet < 0) {
			break;
		}
	}
	g_wav_index++;
	if (g_wav_index > 9999 || g_wav_index < 0) {
		g_wav_index = 0;
	}
	return g_wav_index;
}

//rtc time 
TIME_T	g_current_time;
INT8U time_data[20];
INT32S rtc_time_get_and_start(void)
{
	INT8U  data;
	INT8U  *pdata;
	INT16S fd;
	INT16U wtemp;
	INT32U addr;
	INT32S nRet;
	INT32U type;
	fd = open("C:\\userconfig.txt", O_RDONLY);
	
	if(fd < 0)
	{
		DBG_PRINT("OPEN userconfig.txt FAIL!!!!!\r\n");
		goto Fail_Return;
	}
	
	type = LED_UPDATE_PROGRAM; 	
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
	addr = (INT32U)time_data;
	if(!addr)
		goto Fail_Return;
		
	nRet = read(fd, addr, 20);
	if(nRet <= 0)
		goto Fail_Return;
	
	pdata = (INT8U*)addr;
	//year
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1000;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*100;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	if((wtemp > 2017)|| (wtemp < 2007))
	{
		g_current_time.tm_sec = 0;
		g_current_time.tm_min = 0;
		g_current_time.tm_hour = 0;
		g_current_time.tm_mday = 31;
		g_current_time.tm_mon = 12;
		g_current_time.tm_year = 2007;
		g_current_time.tm_wday = 6;			
		DBG_PRINT("Cal is out of limit!! \r\n ");
		goto Setup_Cal;
	}
	else
	g_current_time.tm_year = wtemp;
	
	//skip -		
	pdata++;	
	
	//month
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	g_current_time.tm_mon = wtemp;
			
	//skip -		
	pdata++;
	
	//day
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	g_current_time.tm_mday = wtemp;
	
	//skip space		
	pdata++;
	
	//hour
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	g_current_time.tm_hour = wtemp;
			
	//skip :	
	pdata++;
			
	//minute
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	g_current_time.tm_min = wtemp;
	
	//skip :	
	pdata++;
			
	//second
	wtemp = 0;
	data = *pdata++;
	data -= 0x30;
	wtemp += data*10;
	
	data = *pdata++;
	data -= 0x30;
	wtemp += data*1;
	g_current_time.tm_sec = wtemp;


Setup_Cal:
	close(fd);
	nRet=unlink("C:\\userconfig.txt");	//added by george 20100125
	if(nRet!=0) DBG_PRINT("Fail to Delete the userconfig.txt.......%d ",nRet);
//	gp_free((void*) addr);
	
	calendar_init();
	cal_time_set(g_current_time);
	cal_time_get(&g_current_time);
	DBG_PRINT("StartTime = %d.%d.%d--%d.%d.%d\r\n", g_current_time.tm_year, g_current_time.tm_mon, g_current_time.tm_mday,
	 		 										g_current_time.tm_hour, g_current_time.tm_min, g_current_time.tm_sec);
	
	
	type = LED_UPDATE_FINISH;
	msgQSend(PeripheralTaskQ, MSG_PERIPHERAL_TASK_LED_SET, &type, sizeof(INT32U), MSG_PRI_NORMAL);
	OSTimeDly(500);
	msgQSend(ApQ, MSG_APQ_POWER_KEY_ACTIVE, NULL, NULL, MSG_PRI_NORMAL); 	//add by lqy 2011.08.24
	return STATUS_OK;

Fail_Return:

	close(fd);
//	gp_free((void*) addr);
			
	calendar_init();
	cal_time_get(&g_current_time);
	cal_time_set(g_current_time);
	return STATUS_FAIL;
}

