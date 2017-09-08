#include "fsystem.h"
#include "globals.h"
#include "avi_encoder_app.h"
//=========================================================
//函数功能：
//    将file2_handle文件连接到file1_handle文件之后
//=========================================================
INT16S flie_cat(INT16S file1_handle, INT16S file2_handle)
{
	REG f_node_ptr fnp1, fnp2;
	INT16S disk; 
	
	INT32U file1_size, file2_size;
	INT32U file1_startclst, file2_startclst;
	INT16U secsize;
	INT16U	ClusterSize;
	INT32U  i, next_clus;
FS_OS_LOCK();	
	if((file1_handle<0)|(file2_handle<0))
	   return DE_INVLDPARM;
	
	fnp1 = xlt_fd(file1_handle);
	if( (fnp1 == (f_node_ptr)0))
		return 	 DE_INVLDHNDL;
	if (!fnp1->f_dpb ->dpb_mounted || !fnp1->f_count)
		return (INT32S) DE_INVLDHNDL;		

	
	fnp2 = xlt_fd(file2_handle);
	if( (fnp2 == (f_node_ptr)0))
		return 	 DE_INVLDHNDL;
    if (!fnp2->f_dpb ->dpb_mounted || !fnp2->f_count)
		return (INT32S) DE_INVLDHNDL;		
  
  //获取cluster size
    secsize = fnp1->f_dpb->dpb_secsize;	// get sector size	
	ClusterSize = secsize << fnp1->f_dpb->dpb_shftcnt;
	disk = fnp1->f_dpb->dpb_unit;		// get disk internal number		
	
	file1_startclst = fnp1->f_dir.dir_start;  //获取文件起始簇
	file2_startclst = fnp2->f_dir.dir_start;  //获取文件起始簇
	
	file1_size = fnp1->f_dir.dir_size;
	file2_size = fnp2->f_dir.dir_size;

	DEBUG_MSG(DBG_PRINT("flie1_stclus = %d\r\n", file1_startclst));	
	DEBUG_MSG(DBG_PRINT("flie2_stclus = %d\r\n", file2_startclst));					
	for(i=file1_startclst;;)
	{
		 next_clus = next_cluster(fnp1->f_dpb, i);
		 if(next_clus==LONG_LAST_CLUSTER)
		   break;
		 
		 i = next_clus;
	}
	DEBUG_MSG(DBG_PRINT("flie1_endclus = %d\r\n", i));	
	link_fat(fnp1->f_dpb,i,file2_startclst);
	
	//if (!flush1(fnp1->f_dpb))
	//		return DE_INVLDBUF;
	//修改文件size
	fnp1->f_dir.dir_size = file1_size+file2_size;
	fnp1->f_flags |= F_DMOD;
	
	flush_buffers(disk);
FS_OS_UNLOCK();
	return 0;
	
}

