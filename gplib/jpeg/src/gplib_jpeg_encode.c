/*
* Purpose: APIs for compress image to JPEG file
*
* Author: Tristan Yang
*
* Date: 2008/06/12
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.00
*/

#include "gplib_jpeg_encode.h"

#if GPLIB_JPEG_ENCODE_EN == 1

static INT16U saved_quant_luminance[64];
static INT16U saved_quant_chrominance[64];
static INT8U saved_lum_quality;
static INT8U saved_chrom_quality;

static INT16U quant50_luminance_table[64] = {
	0x10, 0x0B, 0x0A, 0x10, 0x18, 0x28, 0x33, 0x3D, 0x0C, 0x0C, 0x0E, 0x13, 0x1A, 0x3A, 0x3C, 0x37,
	0x0E, 0x0D, 0x10, 0x18, 0x28, 0x39, 0x45, 0x38, 0x0E, 0x11, 0x16, 0x1D, 0x33, 0x57, 0x50, 0x3E,
	0x12, 0x16, 0x25, 0x38, 0x44, 0x6D, 0x67, 0x4D, 0x18, 0x23, 0x37, 0x40, 0x51, 0x68, 0x71, 0x5C,
	0x31, 0x40, 0x4E, 0x57, 0x67, 0x79, 0x78, 0x65, 0x48, 0x5C, 0x5F, 0x62, 0x70, 0x64, 0x67, 0x63
};

static INT16U quant50_chrominance_table[64] = {
	0x11, 0x12, 0x18, 0x2F, 0x63, 0x63, 0x63, 0x63, 0x12, 0x15, 0x1A, 0x42, 0x63, 0x63, 0x63, 0x63,
	0x18, 0x1A, 0x38, 0x63, 0x63, 0x63, 0x63, 0x63, 0x2F, 0x42, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63
};

static const INT8U zigzag_scan[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21,	28,
	35, 42, 49, 56, 57, 50,	43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static INT8U jpeg_encode_started = 0;

void jpeg_encode_init(void)
{
	jpeg_init();

	jpeg_encode_started = 0;
}

// This API is called to fill quantization table of JPEG header
void jpeg_header_quantization_table_calculate(JPEG_QTABLE_ENUM table, INT32U quality, INT8U *qtable)
{
	INT32U i;
	INT16U *ptr;

	if (quality >= 100) {
		if (table == ENUM_JPEG_LUMINANCE_QTABLE) {
			for (i=0; i<64; i++) {
				*qtable++ = 0x1;
				saved_quant_luminance[i] = 0x1;
			}
			jpeg_quant_luminance_set(0, 64, &saved_quant_luminance[0]);
		} else {
			for (i=0; i<64; i++) {
				*qtable++ = 0x1;
				saved_quant_chrominance[i] = 0x1;
			}
			jpeg_quant_chrominance_set(0, 64, &saved_quant_chrominance[0]);
		}
		return;
	} else if (quality == 50) {
		if (table == ENUM_JPEG_LUMINANCE_QTABLE) {
			ptr = &quant50_luminance_table[0];
			jpeg_quant_luminance_set(0, 64, &quant50_luminance_table[0]);
		} else {
			ptr = &quant50_chrominance_table[0];
			jpeg_quant_chrominance_set(0, 64, &quant50_chrominance_table[0]);
		}
		for (i=0; i<64; i++) {
			*qtable++ = (INT8U) ptr[zigzag_scan[i]];
		}
		return;
	} else if (quality == 0) {
		quality = 30;
	}
	if (table == ENUM_JPEG_LUMINANCE_QTABLE) {
		if (saved_lum_quality != quality) {
			INT32U scale_factor, temp;

		    if (quality < 50) {
		    	scale_factor = 5000 / quality;
		    } else {
		    	scale_factor = 200 - (quality << 1);
		    }

		    for (i=0; i<64; i++) {
		    	// Calculate DC quantization table
				temp = quant50_luminance_table[i] * scale_factor + 50;
				if (temp >= 25500) {
					saved_quant_luminance[i] = 0x00FF;
				} else if (temp <= 199) {
					saved_quant_luminance[i] = 0x0001;
				} else {
					saved_quant_luminance[i] = (INT16U) (temp / 100);
				}
		    }
		    saved_lum_quality = quality;
		}
		ptr = &saved_quant_luminance[0];
		jpeg_quant_luminance_set(0, 64, &saved_quant_luminance[0]);
	} else {
		if (saved_chrom_quality != quality) {
			INT32U scale_factor, temp;

		    if (quality < 50) {
		    	scale_factor = 5000 / quality;
		    } else {
		    	scale_factor = 200 - (quality << 1);
		    }

		    for (i=0; i<64; i++) {
				// Calculate AC quantization table
				temp = quant50_chrominance_table[i] * scale_factor + 50;
				if (temp >= 25500) {
					saved_quant_chrominance[i] = 0x00FF;
				} else if (temp <= 199) {
					saved_quant_chrominance[i] = 0x0001;
				} else {
					saved_quant_chrominance[i] = (INT16U) (temp / 100);
				}
		    }
		    saved_chrom_quality = quality;
		}
		ptr = &saved_quant_chrominance[0];
		jpeg_quant_chrominance_set(0, 64, &saved_quant_chrominance[0]);
	}

	for (i=0; i<64; i++) {
		*qtable++ = (INT8U) ptr[zigzag_scan[i]];
	}
}

INT32S jpeg_encode_input_size_set(INT16U width, INT16U height)
{
	if (!width || !height) {
		return -1;
	}

	return jpeg_image_size_set(width, height);
}

INT32S jpeg_encode_input_format_set(INT32U format)
{
	if (format == C_JPEG_FORMAT_YUV_SEPARATE) {
		return jpeg_yuyv_encode_mode_disable();
	} else if (format == C_JPEG_FORMAT_YUYV) {
		return jpeg_yuyv_encode_mode_enable();
	}

	return -1;
}

INT32S jpeg_encode_yuv_sampling_mode_set(INT32U encode_mode)
{
	if (encode_mode!=C_JPG_CTRL_YUV422 && encode_mode!=C_JPG_CTRL_YUV420) {
		return -1;
	}

	return jpeg_yuv_sampling_mode_set(encode_mode);
}

INT32S jpeg_encode_output_addr_set(INT32U addr)
{
	if (addr & 0xF) {
		DBG_PRINT("jpeg_encode_output_addr_set(): VLC address is not 16-byte alignment\r\n");
		return -1;
	}
	return jpeg_vlc_addr_set(addr);
}

extern void jpeg_huffman_table_set(void);
extern void jpeg_oldwu_function_enable(void);
extern void jpeg_smallfish_function_enable(void);
extern void jpeg_huffman_table_release(void);
void jpeg_enable_scale_x2_engine(void)
{
	jpeg_huffman_table_set();
	jpeg_oldwu_function_enable();
	jpeg_huffman_table_release();
}

void jpeg_enable_scale_x4_engine(void)
{
	jpeg_huffman_table_set();
	jpeg_smallfish_function_enable();
	jpeg_huffman_table_release();
}

INT32S jpeg_encode_once_start(INT32U y_addr, INT32U u_addr, INT32U v_addr)
{
	if ((y_addr & 0x7) || (u_addr & 0x7) || (v_addr & 0x7)) {
		DBG_PRINT("Address is not 8-byte alignment when calling jpeg_encode_once_start()\r\n");
		return -1;
	}

	if (jpeg_encode_started) {
		DBG_PRINT("jpeg_encode_once_start(): JPEG encoding has already started\r\n");
		return -1;
	}
	if (jpeg_yuv_addr_set(y_addr, u_addr, v_addr)) {
		DBG_PRINT("Calling to jpeg_yuv_addr_set() failed\r\n");
		return -1;
	}
	if (jpeg_compression_start()) {
		DBG_PRINT("Calling to jpeg_compression_start() failed\r\n");
		return -1;
	}

	jpeg_encode_started = 1;

	return 0;
}

INT32S jpeg_encode_on_the_fly_start(INT32U y_addr, INT32U u_addr, INT32U v_addr, INT32U len)
{
	if (jpeg_encode_started) {
		// Make sure parameters are at least 8-byte alignment
		if ((y_addr & 0x7) || (u_addr & 0x7) || (v_addr & 0x7) || !len) {
			DBG_PRINT("Parameter is incorrect when calling jpeg_encode_on_the_fly_start()\r\n");
			return -1;
		}
		if (jpeg_multi_yuv_input_restart(y_addr, u_addr, v_addr, len)) {
			DBG_PRINT("Calling to jpeg_multi_yuv_input_restart() failed\r\n");
			return -1;
		}

		return 0;
	} else {
		if (!y_addr || (y_addr & 0x7) || !len) {
			DBG_PRINT("Parameter is incorrect when calling jpeg_encode_on_the_fly_start()\r\n");
			return -1;
		}
		if (jpeg_yuv_addr_set(y_addr, u_addr, v_addr)) {
			DBG_PRINT("Calling to jpeg_yuv_addr_set() failed\r\n");
			return -1;
		}
		if (jpeg_multi_yuv_input_init(len)) {
			DBG_PRINT("Calling to jpeg_multi_yuv_input_init() failed\r\n");
			return -1;
		}
		if (jpeg_compression_start()) {
			DBG_PRINT("Calling to jpeg_compression_start() failed\r\n");
			return -1;
		}
		jpeg_encode_started = 1;
	}

	return 0;
}

void jpeg_encode_stop(void)
{
	jpeg_stop();

	jpeg_encode_started = 0;
}

// Return the current status of JPEG engine
INT32S jpeg_encode_status_query(INT32U wait)
{
	return jpeg_status_polling(wait);
}

INT32U jpeg_encode_vlc_cnt_get(void)
{
	return jpeg_compress_vlc_cnt_get();
}

#endif		// GPLIB_JPEG_ENCODE_EN