/*
* Purpose: JPEG driver/interface
*
* Author: Tristan Yang
*
* Date: 2007/10/09
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.01
*/

#include "drv_l1_jpeg.h"

#if (defined _DRV_L1_JPEG) && (_DRV_L1_JPEG == 1)


#if _OPERATING_SYSTEM != _OS_NONE
#define C_JPEG_QUEUE_SIZE	16
OS_EVENT *jpeg_message_queue = NULL;
void *jpeg_message_buffer[C_JPEG_QUEUE_SIZE];
#else
volatile INT32S jpeg_interrupt_flag = 0;
#endif

// For patch
static INT32U function_id=0, customer_id=0;
static INT8U x2_enable=0, x4_enable=0, x2_patch_skip=0, s720_patch_skip=0;
static INT8U dummy_stuff;

INT8U jpeg_init_done = 0;

INT32S jpeg_calculate_image_size(void);
INT32S jpeg_device_protect(void);
void jpeg_device_unprotect(INT32S mask);
void jpeg_isr(void);

void jpeg_init(void)
{
	R_JPG_INT_CTRL = 0;						// Disable interrupt
	R_JPG_CTRL = 0;							// Stop JPEG engine
	R_JPG_PROGRESSIVE = 0x0;				// Disable progressive mode
	R_JPG_RESET = C_JPG_RESET;				// Reset JPEG Engine
	R_JPG_RESET = 0x0;
	R_JPG_INT_FLAG = C_JPG_INT_MASK;		// Clear pending interrupt flags

	R_JPG_JFIF = C_JPG_STANDARD_FORMAT;
	R_JPG_RESTART_MCU_NUM = 0;
	R_JPG_READ_CTRL = 0;
	R_JPG_VLC_TOTAL_LEN = C_JPG_VLC_TOTAL_LEN_MAX;

  #if _OPERATING_SYSTEM != _OS_NONE
	if (jpeg_init_done) {
    	OSQFlush(jpeg_message_queue);
    } else {
		vic_irq_register(VIC_JPEG, jpeg_isr);
		vic_irq_enable(VIC_JPEG);

    	if ((jpeg_message_queue = OSQCreate(&jpeg_message_buffer[0], C_JPEG_QUEUE_SIZE)) == NULL) {
    		jpeg_init_done = 0;
    	} else {
    		jpeg_init_done = 1;
    	}
	}
  #else
  	if (!jpeg_init_done) {
		vic_irq_register(VIC_JPEG, jpeg_isr);
		vic_irq_enable(VIC_JPEG);
		jpeg_init_done = 1;
	}
  	jpeg_interrupt_flag = 0;
  #endif
  
  	// Init patch here
	if (!function_id) {
		// Make sure clock of UART is enabled so that efuse can be read
		if ((*((volatile INT32U *) 0xD0000010)) & 0x0080) {
			function_id = (*((volatile INT32U *) 0xC0120004));
			customer_id = (*((volatile INT32U *) 0xC012000C));
		} else {
			(*((volatile INT32U *) 0xD0000010)) |= 0x0080;
			function_id = (*((volatile INT32U *) 0xC0120004));
			customer_id = (*((volatile INT32U *) 0xC012000C));
			(*((volatile INT32U *) 0xD0000010)) &= ~0x0080;
		}
	}
}

// Set Huffman table
INT32S jpeg_huffman_dc_lum_mincode_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_LUM_MINCODE) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_dc_lum_valptr_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_LUM_VALPTR) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_dc_lum_huffval_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_LUM_HUFFVAL) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_dc_chrom_mincode_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_CHROM_MINCODE) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_dc_chrom_valptr_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_CHROM_VALPTR) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_dc_chrom_huffval_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_DC_CHROM_HUFFVAL) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_lum_mincode_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_LUM_MINCODE) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_lum_valptr_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_LUM_VALPTR) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_lum_huffval_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 256) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_LUM_HUFFVAL) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_chrom_mincode_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_CHROM_MINCODE) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_chrom_valptr_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 16) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_CHROM_VALPTR) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_huffman_ac_chrom_huffval_set(INT32U offset, INT32U len, INT8U *val)
{
	INT32U *ptr;

	if (offset+len > 256) {
		return -1;
	}
	ptr = ((INT32U *) P_JPG_AC_CHROM_HUFFVAL) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

// Set Quantization table
INT32S jpeg_quant_luminance_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 64) {
		return -1;
	}

	ptr = ((INT32U *) P_JPG_QUANT_LUM) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_quant_chrominance_set(INT32U offset, INT32U len, INT16U *val)
{
	INT32U *ptr;

	if (offset+len > 64) {
		return -1;
	}

	ptr = ((INT32U *) P_JPG_QUANT_CHROM) + offset;
	while (len--) {
		*ptr++ = (INT32U) (*val++);
	}
	return 0;
}

INT32S jpeg_format_set(INT32U format)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	if (format == C_JPG_STANDARD_FORMAT) {
		R_JPG_JFIF |= C_JPG_STANDARD_FORMAT;
	} else {
		R_JPG_JFIF &= ~(C_JPG_STANDARD_FORMAT);
	}

	return 0;
}

INT32S jpeg_restart_interval_set(INT16U interval)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_RESTART_MCU_NUM = (INT32U) interval;

	return 0;
}

INT32S jpeg_calculate_image_size(void)
{
	INT16U width, height;
	INT32U mode, size, scaler_size;

	mode = R_JPG_CTRL & C_JPG_CTRL_MODE_MASK;

	// First, check whether clipping function is used
	if (R_JPG_CTRL & C_JPG_CTRL_CLIPPING_EN) {
		if (R_JPG_CLIP_START_X+R_JPG_CLIP_WIDTH > R_JPG_EXTENDED_HSIZE) {
			return -1;
		}
		if (R_JPG_CLIP_START_Y+R_JPG_CLIP_HEIGHT > R_JPG_EXTENDED_VSIZE) {
			return -1;
		}
		width = R_JPG_CLIP_WIDTH;
		height = R_JPG_CLIP_HEIGHT;
	} else {
		width = R_JPG_EXTENDED_HSIZE;
		height = R_JPG_EXTENDED_VSIZE;
	}

	// Second, calculate size according to YUV mode
	if (R_JPG_CTRL & C_JPG_CTRL_YUYV_ENCODE_EN) {
		if ((mode != C_JPG_CTRL_YUV422) && (mode != C_JPG_CTRL_YUV420)) {
			return -1;
		}
		if ((R_JPG_HSIZE & 0xF) || (R_JPG_VSIZE & 0x7)) {	// Width must be multiple of 16, height must be at least multiple of 8
			return -1;
		}
		size = (width * height) << 1;		// x2
	} else if (R_JPG_CTRL & C_JPG_CTRL_YUYV_DECODE_EN) {
		size = (width * height) << 1;		// x2
		scaler_size = 0;
	} else if (mode == C_JPG_CTRL_YUV422) {
		if (width & 0xF) {					// Width must be multiple of 16
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else if (mode == C_JPG_CTRL_YUV420) {
		if ((width & 0xF) || (height & 0xF)) {		// Must be multiple of 16
			return -1;
		}
		size = width * height;
		scaler_size = size << 1;
		size = (size+scaler_size) >> 1;		// x1.5
	} else if (mode == C_JPG_CTRL_YUV422V) {
		if (height & 0xF) {					// Height must be multiple of 16
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else if (mode == C_JPG_CTRL_YUV444) {
		size = width * height;
		scaler_size = size << 1;
		size += scaler_size;					// x3
	} else if (mode == C_JPG_CTRL_GRAYSCALE) {
		size = width * height;				// x1
		scaler_size = size << 1;
	} else if (mode == C_JPG_CTRL_YUV411) {
		if (width & 0x1F) {					// Width must be multiple of 32
			return -1;
		}
		size = width * height;
		scaler_size = size << 1;
		size = (size+scaler_size) >> 1;		// x1.5
	} else if (mode == C_JPG_CTRL_YUV411V) {
		if (height & 0x1F) {				// Height must be multiple of 32
			return -1;
		}
		size = width * height;
		scaler_size = size << 1;
		size = (size+scaler_size) >> 1;		// x1.5
	} else if (mode == C_JPG_CTRL_YUV420H2) {
		if ((width & 0xF) || (height & 0xF)) {		// Must be multiple of 16
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else if (mode == C_JPG_CTRL_YUV420V2) {
		if ((width & 0xF) || (height & 0xF)) {		// Must be multiple of 16
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else if (mode == C_JPG_CTRL_YUV411H2) {
		if (width & 0x1F) {					// Width must be multiple of 32
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else if (mode == C_JPG_CTRL_YUV411V2) {
		if (height & 0x1F) {				// Height must be multiple of 32
			return -1;
		}
		size = (width * height) << 1;		// x2
		scaler_size = size;
	} else {
		return -1;
	}

	// Third, check whether scale-down mode is used
	if (R_JPG_CTRL & C_JPG_CTRL_LEVEL8_SCALEDOWN_EN) {
		size >>= 6;							// Level8 scale-down size is 1/64 of the original image
	}
	if (R_JPG_CTRL & C_JPG_CTRL_LEVEL2_SCALEDOWN_EN) {
		size >>= 2;							// YUV-separate level2 scale-down size is 1/4 of the original image
	}
	if (R_JPG_CTRL & C_JPG_CTRL_YUYV_L2_SCALEDOWN_EN) {
		R_JPG_CTRL |= C_JPG_CTRL_IMAGE_SIZE_SET;
		size >>= 2;							// YUYV level2 scale-down size is 1/4 of the original image
	} else if (R_JPG_CTRL & C_JPG_CTRL_YUYV_L4_SCALEDOWN_EN) {
		R_JPG_CTRL |= C_JPG_CTRL_IMAGE_SIZE_SET;
		size >>= 4;							// YUYV level4 scale-down size is 1/16 of the original image
	}
	if (!size || (size>C_JPG_IMAGE_SIZE_MAX)) {
		return -1;
	}

	// Final step, write the image size to register
	R_JPG_IMAGE_SIZE = size;
//	if(scaler_size) {
//        if (R_JPG_CTRL & C_JPG_CTRL_YUYV_DECODE_EN) {
//	        R_JPG_SCALER_IMAGE_SIZE = scaler_size;
//        }
//	}

	return 0;
}

INT32S jpeg_image_size_set(INT16U hsize, INT16U vsize)
{
	INT32S ret;

	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	ret = 0;
	if (hsize > C_JPG_HSIZE_MAX) {
		hsize = C_JPG_HSIZE_MAX;
		ret = -1;
	}
	if (vsize > C_JPG_VSIZE_MAX) {
		vsize = C_JPG_VSIZE_MAX;
		ret = -1;
	}

	// Set width and height
	R_JPG_HSIZE = (INT32U) hsize;
	R_JPG_VSIZE = (INT32U) vsize;

	return ret;
}

INT32S jpeg_yuv_sampling_mode_set(INT32U mode)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	if (mode!=C_JPG_CTRL_YUV422 &&
		mode!=C_JPG_CTRL_YUV420 &&
		mode!=C_JPG_CTRL_YUV422V &&
		mode!=C_JPG_CTRL_YUV444 &&
		mode!=C_JPG_CTRL_GRAYSCALE &&
		mode!=C_JPG_CTRL_YUV411 &&
		mode!=C_JPG_CTRL_YUV411V &&
		mode!=C_JPG_CTRL_YUV420H2 &&
		mode!=C_JPG_CTRL_YUV420V2 &&
		mode!=C_JPG_CTRL_YUV411H2 &&
		mode!=C_JPG_CTRL_YUV411V2) {
		return -1;
	}

	// Set YUV mode
	R_JPG_CTRL &= ~(C_JPG_CTRL_MODE_MASK);
	R_JPG_CTRL |= mode;

	return 0;
}

INT32S jpeg_level2_scaledown_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	//R_JPG_CTRL |= C_JPG_CTRL_LEVEL2_SCALEDOWN_EN;
	R_JPG_CTRL |= C_JPG_CTRL_YUYV_L2_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_level2_scaledown_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	//R_JPG_CTRL &= ~(C_JPG_CTRL_LEVEL2_SCALEDOWN_EN | C_JPG_CTRL_SCALE_H_EVEN | C_JPG_CTRL_SCALE_V_EVEN);
	R_JPG_CTRL &= ~C_JPG_CTRL_YUYV_L2_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_level4_scaledown_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL |= C_JPG_CTRL_YUYV_L4_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_level4_scaledown_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL &= ~C_JPG_CTRL_YUYV_L4_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_level8_scaledown_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL |= C_JPG_CTRL_LEVEL8_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_level8_scaledown_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL &= ~C_JPG_CTRL_LEVEL8_SCALEDOWN_EN;

	return 0;
}

INT32S jpeg_progressive_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_PROGRESSIVE = 0x7;

	return 0;
}

INT32S jpeg_clipping_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL |= C_JPG_CTRL_CLIPPING_EN;

	return 0;
}

INT32S jpeg_clipping_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL &= ~C_JPG_CTRL_CLIPPING_EN;

	return 0;
}

INT32S jpeg_image_clipping_range_set(INT16U start_x, INT16U start_y, INT16U width, INT16U height)
{
	INT32S ret;

	ret = 0;
	// Make sure parameters are 8-byte alignment
	if ((start_x & 0x7) || (start_y & 0x7) || (width & 0x7) || (height & 0x7)) {
		start_x &= ~0x7;
		start_y &= ~0x7;
		width &= ~0x7;
		height &= ~0x7;
		ret = -1;
	}
	if (start_x > C_JPG_HSIZE_MAX) {
		start_x = 0;
		ret = -1;
	}
	if (start_y > C_JPG_VSIZE_MAX) {
		start_y = 0;
		ret = -1;
	}
	if (start_x+width > C_JPG_HSIZE_MAX) {
		width = C_JPG_HSIZE_MAX - start_x;
		width &= ~0x7;
		ret = -1;
	}
	if (start_y+height > C_JPG_VSIZE_MAX) {
		height = C_JPG_VSIZE_MAX - start_y;
		height &= ~0x7;
		ret = -1;
	}

	R_JPG_CLIP_START_X = start_x;
	R_JPG_CLIP_START_Y = start_y;
	R_JPG_CLIP_WIDTH = width;
	R_JPG_CLIP_HEIGHT = height;

	return ret;
}

INT32S jpeg_progressive_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_PROGRESSIVE = 0x0;

	return 0;
}

INT32S jpeg_using_scaler_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL |= C_JPG_CTRL_YUYV_DECODE_EN;

	return 0;
}

INT32S jpeg_using_scaler_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL &= ~C_JPG_CTRL_YUYV_DECODE_EN;

	return 0;
}

INT32S jpeg_yuyv_encode_mode_enable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL |= C_JPG_CTRL_YUYV_ENCODE_EN;

	return 0;
}

INT32S jpeg_yuyv_encode_mode_disable(void)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_CTRL &= ~C_JPG_CTRL_YUYV_ENCODE_EN;

	return 0;
}

INT32S jpeg_yuv_addr_set(INT32U y_addr, INT32U u_addr, INT32U v_addr)
{
	INT32S ret;

	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	ret = 0;
	// Make sure parameters are at least 8-byte alignment
	if ((y_addr & 0x7) || (u_addr & 0x7) || (v_addr & 0x7)) {
		y_addr &= ~0x7;
		u_addr &= ~0x7;
		v_addr &= ~0x7;
		ret = -1;
	}

	R_JPG_Y_FRAME_ADDR = y_addr;
	R_JPG_U_FRAME_ADDR = u_addr;
	R_JPG_V_FRAME_ADDR = v_addr;

	return ret;
}

// Compression: When YUYV mode is used, len should be the length of first YUYV data that will be compressed. It should be sum of Y,U and V data when YUV separate mode is used
INT32S jpeg_multi_yuv_input_init(INT32U len)
{
	INT32S ret;

	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	ret = 0;
	if (len > C_JPG_READ_ON_FLY_LEN_MAX) {
		len = C_JPG_READ_ON_FLY_LEN_MAX;
		ret = -1;
	}

	R_JPG_READ_CTRL = C_JPG_READ_CTRL_EN | len;

	R_JPG_INT_FLAG = C_JPG_INT_READ_EMPTY_PEND;		// Clear pending bit
	R_JPG_INT_CTRL |= C_JPG_INT_READ_EMPTY_EN;

	return ret;
}

// Compression: output VLC address, addr must be 16-byte alignment. Decompression: input VLC address
INT32S jpeg_vlc_addr_set(INT32U addr)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	R_JPG_VLC_ADDR = addr;

	return 0;
}

// Decompression: JPEG engine stops when maximum VLC bytes are read(Maximum=0x03FFFFFF)
INT32S jpeg_vlc_maximum_length_set(INT32U len)
{
	INT32S ret;

	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	ret = 0;
	if (len > C_JPG_VLC_TOTAL_LEN_MAX) {
		len = C_JPG_VLC_TOTAL_LEN_MAX;
		ret = -1;
	}
	R_JPG_VLC_TOTAL_LEN = len;

	return ret;
}

// Decompression: length(Maximum=0x000FFFFF) of first VLC buffer
INT32S jpeg_multi_vlc_input_init(INT32U len)
{
	INT32S ret;

	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	ret = 0;
	if (len > C_JPG_READ_ON_FLY_LEN_MAX) {
		len = C_JPG_READ_ON_FLY_LEN_MAX;
		ret = -1;
	}

	R_JPG_READ_CTRL = C_JPG_READ_CTRL_EN | C_JPG_READ_ADDR_UPDATE | len;

	R_JPG_INT_FLAG = C_JPG_INT_READ_EMPTY_PEND;		// Clear pending bit
	R_JPG_INT_CTRL |= C_JPG_INT_READ_EMPTY_EN;

	return ret;
}

// Decompression: FIFO line number, JPEG engine stops when the specified lines are output. User can restart JPEG engine by calling jpeg_yuv_output_restart()
INT32S jpeg_fifo_line_set(INT32U line)
{
	// Not allowed to change setting when JPEG is working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	if ((line!=C_JPG_FIFO_DISABLE) &&
		(line!=C_JPG_FIFO_16LINE) &&
		(line!=C_JPG_FIFO_32LINE) &&
		(line!=C_JPG_FIFO_64LINE) &&
		(line!=C_JPG_FIFO_128LINE) &&
		(line!=C_JPG_FIFO_256LINE)) {
		return -1;
	}

	R_JPG_RING_FIFO &= ~(C_JPG_FIFO_LINE_MASK);
	R_JPG_RING_FIFO |= line;

	R_JPG_INT_FLAG = C_JPG_INT_DECODE_OUT_FULL_PEND;		// Clear pending bit
	if (line != C_JPG_FIFO_DISABLE) {
	  	R_JPG_INT_CTRL |= C_JPG_INT_DECODE_OUT_FULL_EN;
	} else {
		R_JPG_INT_CTRL &= ~C_JPG_INT_DECODE_OUT_FULL_EN;
	}
	return 0;
}

void jpeg_huffman_table_set(void)
{
	dummy_stuff ^= (*((volatile INT32U *) 0xD0500380));
}

void jpeg_huffman_table_release(void)
{
	dummy_stuff ^= (*((volatile INT32U *) 0xD0500384));
}

void jpeg_oldwu_function_enable(void)
{
	x2_enable = 1;
}

void jpeg_oldwu_patch_enable(void)
{
	x2_patch_skip = 1;
}

void jpeg_smallfish_function_enable(void)
{
	x4_enable = 1;
}

void jpeg_bigmac_enable(void)
{
	s720_patch_skip = 1;
}

INT32S jpeg_compression_start(void)
{
  	if (!jpeg_init_done) {
  		return -1;
  	}
	// Not allowed to start compression when JPEG is still working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}

	// Scale-down, clipping and using-scaler modes are not allowed in compression operation
	R_JPG_CTRL &= ~(C_JPG_CTRL_LEVEL8_SCALEDOWN_EN | C_JPG_CTRL_CLIPPING_EN | C_JPG_CTRL_YUYV_DECODE_EN | C_JPG_CTRL_LEVEL2_SCALEDOWN_EN);
  	if (jpeg_calculate_image_size()) {
  		return -1;
  	}

  	R_JPG_INT_FLAG = R_JPG_INT_FLAG;		// Clear pending bit

  	R_JPG_INT_CTRL |= (C_JPG_INT_ENCODE_OUT_FULL_EN | C_JPG_INT_COMP_EN);

	// Vertical 9/8 calibration for 640x432 to 720x480
	if (R_JPG_HSIZE==720 && (R_CSI_TG_CTRL0 & (1<<17))) {
		R_JPG_CTRL |= 0x8 << 19;
	} else if (x2_enable) {		// 2X scale up
		R_JPG_IMAGE_SIZE = (R_JPG_HSIZE*R_JPG_VSIZE) << 1;
		
		if ((R_JPG_CTRL & C_JPG_CTRL_MODE_MASK) == C_JPG_CTRL_YUV422) {		// YUV422		
			R_JPG_CTRL |= 0x00080000;
		} else {	// YUV420
			if (R_JPG_HSIZE > 1440) {	
				R_JPG_CTRL |= 0x00880000;	// Duplicate 2X
			} else {
				R_JPG_CTRL |= 0x00980000;	// Bilinear 2X supports up to 720 pixels in width only
			}
		}
	} else if (x4_enable) {		// 4X scale up
		R_JPG_IMAGE_SIZE = (R_JPG_HSIZE*R_JPG_VSIZE) << 1;
		
		if (R_JPG_HSIZE > 1440) {
			return -1;
		}
		if ((R_JPG_CTRL & C_JPG_CTRL_MODE_MASK) == C_JPG_CTRL_YUV422) {		// YUV422		
			R_JPG_CTRL |= 0x00200000;
		} else {
			return -1;
		}
	}

	// Check patch here
	if (1) {
		if (!function_id) {
			// Make sure clock of UART is enabled so that efuse can be read
			if ((*((volatile INT32U *) 0xD0000010)) & 0x0080) {
				function_id = (*((volatile INT32U *) 0xC0120004));
				customer_id = (*((volatile INT32U *) 0xC012000C));
			} else {
				(*((volatile INT32U *) 0xD0000010)) |= 0x0080;
				function_id = (*((volatile INT32U *) 0xC0120004));
				customer_id = (*((volatile INT32U *) 0xC012000C));
				(*((volatile INT32U *) 0xD0000010)) &= ~0x0080;
			}
		}
		
		if ((R_JPG_CTRL & 0x280000) && !(function_id & 0x2000) && (customer_id!=0x6FFF) && !x2_patch_skip) {
			// Disable 2X and 4X function
			R_JPG_CTRL &= ~0x280000;
				
			return 0;
		}
	}

	// Start compression
	R_JPG_CTRL |= C_JPG_CTRL_YCBCR_FORMAT | C_JPG_CTRL_COMPRESS;

	return 0;
}

// JPEG will stop when input YUV buffer is empty, clear the pending bit to restart the JPEG compression engine
INT32S jpeg_multi_yuv_input_restart(INT32U y_addr, INT32U u_addr, INT32U v_addr, INT32U len)
{
	INT32S mask;
	INT32S ret;

	if (!(R_JPG_CTRL & C_JPG_CTRL_COMPRESS)) {
		return -1;
	}
	if (!(R_JPG_READ_CTRL & C_JPG_READ_CTRL_EN)) {
		return -1;
	}
	ret = 0;
	if ((y_addr & 0x7) || (u_addr & 0x7) || (v_addr & 0x7)) {
		y_addr &= ~(0x7);
		u_addr &= ~(0x7);
		v_addr &= ~(0x7);
		ret = -1;
	}
	if (len > C_JPG_READ_ON_FLY_LEN_MAX) {
		len = C_JPG_READ_ON_FLY_LEN_MAX;
		ret = -1;
	}

	R_JPG_Y_FRAME_ADDR = y_addr;
	R_JPG_U_FRAME_ADDR = u_addr;
	R_JPG_V_FRAME_ADDR = v_addr;
	R_JPG_READ_CTRL = C_JPG_READ_CTRL_EN | len;

  	R_JPG_INT_FLAG = C_JPG_INT_READ_EMPTY_PEND;		// Clear pending bit

	mask = jpeg_device_protect();
  	R_JPG_INT_CTRL |= C_JPG_INT_READ_EMPTY_EN;
  	jpeg_device_unprotect(mask);

  	return ret;
}

INT32S jpeg_decompression_start(void)
{
  	if (!jpeg_init_done) {
  		return -1;
  	}
	// Not allowed to start decompression when JPEG is still working
	if (R_JPG_CTRL & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	// Level2 and level8 scale-down mode are not allowed to enabled at the same time
	if ((R_JPG_CTRL&(C_JPG_CTRL_LEVEL8_SCALEDOWN_EN|C_JPG_CTRL_LEVEL2_SCALEDOWN_EN)) == (C_JPG_CTRL_LEVEL8_SCALEDOWN_EN|C_JPG_CTRL_LEVEL2_SCALEDOWN_EN)) {
		return -1;
	}
	// JPEG using scaler engine mode should not be used with other special modes
//	if ((R_JPG_CTRL&C_JPG_CTRL_YUYV_DECODE_EN) && (R_JPG_CTRL&(C_JPG_CTRL_LEVEL8_SCALEDOWN_EN|C_JPG_CTRL_CLIPPING_EN|C_JPG_CTRL_LEVEL2_SCALEDOWN_EN))) {
//		return -1;
//	}

	// YUYV encode mode is not allowed in decompression
	R_JPG_CTRL &= ~C_JPG_CTRL_YUYV_ENCODE_EN;
  	if (jpeg_calculate_image_size()) {
  		return -1;
  	}

  	R_JPG_INT_FLAG = C_JPG_INT_DECOMP_PEND;		// Clear pending bit

  	R_JPG_INT_CTRL |= C_JPG_INT_DECOMP_EN;

	// Start decompression
	R_JPG_CTRL |= C_JPG_CTRL_YCBCR_FORMAT | C_JPG_CTRL_DECOMPRESS;

	return 0;
}

// JPEG will stop when input VLC buffer is empty, clear the pending bit to restart the JPEG decompression engine
INT32S jpeg_multi_vlc_input_restart(INT32U addr, INT32U len)
{
	INT32S mask;
	INT32S ret;

	if (!(R_JPG_CTRL & C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	if (!(R_JPG_READ_CTRL & C_JPG_READ_CTRL_EN)) {
		return -1;
	}
	ret = 0;
	if (addr & 0xF) {						// All VLC addresses must align to 16-byte boundary except the first one
		addr &= ~0xF;
		ret = -1;
	}
	if (len > C_JPG_READ_ON_FLY_LEN_MAX) {
		len = C_JPG_READ_ON_FLY_LEN_MAX;
		ret = -1;
	}

	R_JPG_VLC_ADDR = addr;
	R_JPG_READ_CTRL = C_JPG_READ_CTRL_EN | C_JPG_READ_ADDR_UPDATE | len;

  	R_JPG_INT_FLAG = C_JPG_INT_READ_EMPTY_PEND;		// Clear pending bit

	mask = jpeg_device_protect();
  	R_JPG_INT_CTRL |= C_JPG_INT_READ_EMPTY_EN;
  	jpeg_device_unprotect(mask);

  	return ret;
}

// If FIFO output mode is used, JPEG will stop when output FIFO is full. Clear the pending bit to restart the JPEG decompression engine
INT32S jpeg_yuv_output_restart(void)
{
	INT32S mask;

	if (!(R_JPG_CTRL & C_JPG_CTRL_DECOMPRESS)) {
		return -1;
	}
	if (!(R_JPG_RING_FIFO & C_JPG_FIFO_ENABLE)) {
		return -1;
	}

	R_JPG_INT_FLAG = C_JPG_INT_DECODE_OUT_FULL_PEND;		// Clear pending bit, JPEG will output to next FIFO

	mask = jpeg_device_protect();
  	R_JPG_INT_CTRL |= C_JPG_INT_DECODE_OUT_FULL_EN;
	jpeg_device_unprotect(mask);

  	return 0;
}

void jpeg_stop(void)
{
	INT32S mask;

	mask = jpeg_device_protect();
	R_JPG_INT_CTRL = 0;						// Disable interrupt
	jpeg_device_unprotect(mask);
	
	// Set JPEG engine stop bit and then polling it untill cleared. This can force JPEG compressing process to break
	R_JPG_RESET = C_JPG_STOP;
	while (R_JPG_RESET & C_JPG_STOP) ;	
	
	R_JPG_CTRL = 0;							// Stop JPEG engine
	R_JPG_PROGRESSIVE = 0x0;

	// Reset JPEG Engine
	R_JPG_RESET = C_JPG_RESET;
	R_JPG_RESET = 0x0;
	
  #if _OPERATING_SYSTEM == _OS_NONE
	jpeg_interrupt_flag = 0;
  #endif

	// Clear pending interrupt flags
	R_JPG_INT_FLAG = C_JPG_INT_MASK;
	
	x2_enable = 0;
	x4_enable = 0;
}

INT32S jpeg_device_protect(void)
{
	return vic_irq_disable(VIC_JPEG);
}

void jpeg_device_unprotect(INT32S mask)
{
	if (mask == 0) {						// Clear device interrupt mask
		vic_irq_enable(VIC_JPEG);
	} else if (mask == 1) {
		vic_irq_disable(VIC_JPEG);
	} else {								// Something is wrong, do nothing
		return;
	}
}

void jpeg_isr(void)							// Device ISR
{
	INT32U pending;
	INT32S message;

	pending = R_JPG_INT_FLAG;
	message = 0;

	// Compression completed
	if ((pending & C_JPG_INT_COMP_PEND) && (R_JPG_INT_CTRL & C_JPG_INT_COMP_EN)) {
	  	message |= C_JPG_STATUS_ENCODE_DONE;
		R_JPG_INT_FLAG = C_JPG_INT_COMP_PEND;		// Clear pending bit
	}
	if ((pending & C_JPG_INT_ENCODE_OUT_FULL_PEND) && (R_JPG_INT_CTRL & C_JPG_INT_ENCODE_OUT_FULL_EN)) {
	  	// Disable this interrupt event. Clear interrupt flag will start decode output again
	  	message |= C_JPG_STATUS_OUTPUT_FULL;
	  	R_JPG_INT_CTRL &= ~C_JPG_INT_ENCODE_OUT_FULL_EN;
	}

	// Decompression completed
	if ((pending & C_JPG_INT_DECOMP_PEND) && (R_JPG_INT_CTRL & C_JPG_INT_DECOMP_EN)) {
//		if (!(R_JPG_CTRL&C_JPG_CTRL_YUYV_DECODE_EN)) {		// If using scaler mode is used, we should only handle interrupt from scaler engine
			message |= C_JPG_STATUS_DECODE_DONE;
//		}
		R_JPG_INT_FLAG = C_JPG_INT_DECOMP_PEND;		// Clear pending bit
	}

	// Decode output FIFO buffer is full
	if ((pending & C_JPG_INT_DECODE_OUT_FULL_PEND) && (R_JPG_INT_CTRL & C_JPG_INT_DECODE_OUT_FULL_EN)) {
	  	message |= C_JPG_STATUS_OUTPUT_FULL;
	  	R_JPG_INT_CTRL &= ~C_JPG_INT_DECODE_OUT_FULL_EN;
	}

	// VLC input buffer is empty
	if ((pending & C_JPG_INT_READ_EMPTY_PEND) && (R_JPG_INT_CTRL & C_JPG_INT_READ_EMPTY_EN)) {
		message |= C_JPG_STATUS_INPUT_EMPTY;
	  	R_JPG_INT_CTRL &= ~C_JPG_INT_READ_EMPTY_EN;
	}

	// Finish reading all VLC data
	if (pending & C_JPG_INT_RST_VLC_DONE_PEND) {
		message |= C_JPG_STATUS_RST_VLC_DONE;
		R_JPG_INT_FLAG = C_JPG_INT_RST_VLC_DONE_PEND;
	}

	if (pending & C_JPG_INT_RST_ERR_PEND) {
		message |= C_JPG_STATUS_RST_MARKER_ERR;
		R_JPG_INT_FLAG = C_JPG_INT_RST_ERR_PEND;
	}

  #if _OPERATING_SYSTEM != _OS_NONE
	if (message) {
		OSQPost(jpeg_message_queue, (void *) message);
	}
  #else
  	jpeg_interrupt_flag |= message;
  #endif
}

void jpeg_using_scaler_decode_done(void)
{
  #if _OPERATING_SYSTEM != _OS_NONE
	OSQPost(jpeg_message_queue, (void *) C_JPG_STATUS_SCALER_DONE);
  #else
  	jpeg_interrupt_flag |= C_JPG_STATUS_SCALER_DONE;
  #endif
}

INT32S jpeg_status_polling(INT32U wait)
{
	INT32U ctrl, mode;
  	INT32S message;
  #if _OPERATING_SYSTEM != _OS_NONE
  	INT8U error;
  #else
  	INT32S mask_jpeg, mask_scaler;
  #endif

  	if (!jpeg_init_done) {
  		return C_JPG_STATUS_INIT_ERR;
  	}

	ctrl = R_JPG_CTRL;
	if (!(ctrl & (C_JPG_CTRL_COMPRESS | C_JPG_CTRL_DECOMPRESS))) {
		return C_JPG_STATUS_STOP;
	}

  #if _OPERATING_SYSTEM != _OS_NONE
  	if (wait) {
  		if (R_JPG_CTRL & C_JPG_CTRL_DECOMPRESS) {
  			if (R_JPG_READ_CTRL & C_JPG_READ_CTRL_EN) {
  				message = (INT32S) OSQPend(jpeg_message_queue, OS_TICKS_PER_SEC, &error);
  			} else if ((R_JPG_RING_FIFO & C_JPG_FIFO_LINE_MASK) == C_JPG_FIFO_DISABLE) {
  				message = (INT32S) OSQPend(jpeg_message_queue, 3*OS_TICKS_PER_SEC, &error);
  			} else if ((R_JPG_RING_FIFO & C_JPG_FIFO_LINE_MASK) == C_JPG_FIFO_256LINE) {
  				message = (INT32S) OSQPend(jpeg_message_queue, 2*OS_TICKS_PER_SEC, &error);
  			} else if (R_JPG_CTRL & C_JPG_CTRL_CLIPPING_EN) {
  				message = (INT32S) OSQPend(jpeg_message_queue, 2*OS_TICKS_PER_SEC, &error);
  			} else {
  				message = (INT32S) OSQPend(jpeg_message_queue, OS_TICKS_PER_SEC, &error);
  			}
  		} else {							// Compression
			message = (INT32S) OSQPend(jpeg_message_queue, 2*OS_TICKS_PER_SEC, &error);
		}
	} else {
		OSQAccept(jpeg_message_queue, &error);
	}
	if (error == OS_NO_ERR) {
		if (message & C_JPG_STATUS_RST_VLC_DONE) {
			if (message & C_JPG_STATUS_DECODE_DONE) {
				// If decode complete successfully, just clear C_JPG_STATUS_RST_VLC_DONE flag
				message &= ~C_JPG_STATUS_RST_VLC_DONE;
			} else {
				// If we get enough restart markers, just clear C_JPG_STATUS_RST_VLC_DONE flag
				mode = R_JPG_CTRL & C_JPG_CTRL_MODE_MASK;
				if (mode==C_JPG_CTRL_YUV422 || mode==C_JPG_CTRL_YUV422V) {
					// 4 blocks of 3 components
					if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>8)) {
						message &= ~C_JPG_STATUS_RST_VLC_DONE;
					}
				} else if (mode==C_JPG_CTRL_YUV420 || mode==C_JPG_CTRL_YUV411 || mode==C_JPG_CTRL_YUV411V) {
					// 6 blocks of 3 components
					if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM*6 >= (R_JPG_IMAGE_SIZE>>6)) {
						message &= ~C_JPG_STATUS_RST_VLC_DONE;
					}
				} else if (mode==C_JPG_CTRL_YUV444) {
					// 3 blocks of 3 components
					if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM*3 >= (R_JPG_IMAGE_SIZE>>6)) {
						message &= ~C_JPG_STATUS_RST_VLC_DONE;
					}
				} else if (mode==C_JPG_CTRL_YUV420H2 || mode==C_JPG_CTRL_YUV420V2 || mode==C_JPG_CTRL_YUV411H2 || mode==C_JPG_CTRL_YUV411V2) {
					// 8 blocks of 3 components
					if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>9)) {
						message &= ~C_JPG_STATUS_RST_VLC_DONE;
					}
				} else {
					if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>6)) {
						message &= ~C_JPG_STATUS_RST_VLC_DONE;
					}
				}
			}
		}
		return message;
	} else {
		if (error == OS_TIMEOUT) {
			return C_JPG_STATUS_TIMEOUT;
		}
		return C_JPG_STATUS_INIT_ERR;
	}
  #else
  	if (wait) {
		while (!jpeg_interrupt_flag) ;		// TBD: we need a timeout mechanism here
	}

	mask_scaler = scaler_device_protect();
	mask_jpeg = jpeg_device_protect();
	message = jpeg_interrupt_flag;
	jpeg_interrupt_flag = 0;
	jpeg_device_unprotect(mask_jpeg);
	scaler_device_unprotect(mask_scaler);

	if (message & C_JPG_STATUS_RST_VLC_DONE) {
		if (message & C_JPG_STATUS_DECODE_DONE) {
			// If decode complete successfully, just clear C_JPG_STATUS_RST_VLC_DONE flag
			message &= ~C_JPG_STATUS_RST_VLC_DONE;
		} else {
			// If we get enough restart markers, just clear C_JPG_STATUS_RST_VLC_DONE flag
			mode = R_JPG_CTRL & C_JPG_CTRL_MODE_MASK;
			if (mode==C_JPG_CTRL_YUV422 || mode==C_JPG_CTRL_YUV422V) {
				// 4 blocks of 3 components
				if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>8)) {
					message &= ~C_JPG_STATUS_RST_VLC_DONE;
				}
			} else if (mode==C_JPG_CTRL_YUV420 || mode==C_JPG_CTRL_YUV411 || mode==C_JPG_CTRL_YUV411V) {
				// 6 blocks of 3 components
				if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM*6 >= (R_JPG_IMAGE_SIZE>>6)) {
					message &= ~C_JPG_STATUS_RST_VLC_DONE;
				}
			} else if (mode==C_JPG_CTRL_YUV444) {
				// 3 blocks of 3 components
				if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM*3 >= (R_JPG_IMAGE_SIZE>>6)) {
					message &= ~C_JPG_STATUS_RST_VLC_DONE;
				}
			} else if (mode==C_JPG_CTRL_YUV420H2 || mode==C_JPG_CTRL_YUV420V2 || mode==C_JPG_CTRL_YUV411H2 || mode==C_JPG_CTRL_YUV411V2) {
				// 8 blocks of 3 components
				if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>9)) {
					message &= ~C_JPG_STATUS_RST_VLC_DONE;
				}
			} else {
				if ((R_JPG_RESTART_MARKER_CNT+1)*R_JPG_RESTART_MCU_NUM >= (R_JPG_IMAGE_SIZE>>6)) {
					message &= ~C_JPG_STATUS_RST_VLC_DONE;
				}
			}
		}
	}

	return message;
  #endif
}

INT32U jpeg_compress_vlc_cnt_get(void)
{
	return R_JPG_ENCODE_VLC_CNT;
}

#endif		// _DRV_L1_JPEG
