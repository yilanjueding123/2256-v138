/*
* Purpose: APIs for decompress Motion-JPEG image
*
* Author: Tristan Yang
*
* Date: 2007/12/27
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.01
*/

#include "gplib_mjpeg_decode.h"

#if GPLIB_JPEG_DECODE_EN == 1

static INT8U mjpeg_decode_started = 0;

void mjpeg_decode_init(void)
{
	// Initiate JPEG engine
	jpeg_decode_init();

	// Initiate Scaler engine
	scaler_init();

	mjpeg_decode_started = 0;
}

INT32S mjpeg_decode_output_addr_set(INT32U y_addr, INT32U u_addr, INT32U v_addr)
{
	return scaler_output_addr_set(y_addr, u_addr, v_addr);
}

INT32S mjpeg_decode_output_format_set(INT32U format)
{
	return scaler_output_format_set(format);
}

// Decodes complete JPEG VLC(variable length coding)
INT32S mjpeg_decode_once_start(INT8U *buf, INT32U len)
{
	INT16U mode;

	if (mjpeg_decode_started) {
		DBG_PRINT("mjpeg_decode_once_start(): JPEG decoding has already started\r\n");
		return -1;
	}

	scaler_output_pixels_set(1<<16, 1<<16, (INT32U) jpeg_decode_image_width_get(), (INT32U) jpeg_decode_image_height_get());
	scaler_fifo_line_set(C_SCALER_CTRL_FIFO_DISABLE);
	mode = jpeg_decode_image_yuv_mode_get();
	if (mode == C_JPG_CTRL_YUV422) {
		scaler_input_format_set(C_SCALER_CTRL_IN_YUV422);
	} else if (mode == C_JPG_CTRL_YUV420) {
		scaler_input_format_set(C_SCALER_CTRL_IN_YUV420);
	} else {
		return -1;
	}
	scaler_YUV_type_set(C_SCALER_CTRL_TYPE_YCBCR);
	scaler_bypass_zoom_mode_enable();

	jpeg_using_scaler_mode_enable();
	jpeg_fifo_line_set(C_JPG_FIFO_DISABLE);

	if (jpeg_vlc_addr_set((INT32U) buf)) {
		DBG_PRINT("Calling to jpeg_vlc_addr_set() failed\r\n");
		return -1;
	}
	if (jpeg_vlc_maximum_length_set(len)) {
		DBG_PRINT("Calling to jpeg_vlc_maximum_length_set() failed\r\n");
		return -1;
	}
	if (jpeg_decompression_start()) {
		DBG_PRINT("Calling to jpeg_decompression_start() failed\r\n");
		return -1;
	}
	mjpeg_decode_started = 1;

	return 0;
}

// Decodes a sequence of JPEG VLC(variable length coding) buffers
INT32S mjpeg_decode_on_the_fly_start(INT8U *buf, INT32U len)
{
	INT16U mode;

	if (mjpeg_decode_started) {
		if ((INT32U) buf & 0xF) {
			DBG_PRINT("Parameter buf is not 16-byte alignment in mjpeg_decode_on_the_fly_start()\r\n");
			return -1;
		} else if (jpeg_multi_vlc_input_restart((INT32U) buf, len)) {
			DBG_PRINT("Calling to jpeg_multi_vlc_input_restart() failed\r\n");
			return -1;
		}

		return 0;
	} else {
		scaler_output_pixels_set(1<<16, 1<<16, (INT32U) jpeg_decode_image_width_get(), (INT32U) jpeg_decode_image_height_get());
		scaler_fifo_line_set(C_SCALER_CTRL_FIFO_DISABLE);
		mode = jpeg_decode_image_yuv_mode_get();
		if (mode == C_JPG_CTRL_YUV422) {
			scaler_input_format_set(C_SCALER_CTRL_IN_YUV422);
		} else if (mode == C_JPG_CTRL_YUV420) {
			scaler_input_format_set(C_SCALER_CTRL_IN_YUV420);
		} else {
			return -1;
		}
		scaler_YUV_type_set(C_SCALER_CTRL_TYPE_YCBCR);
		scaler_bypass_zoom_mode_enable();

		jpeg_using_scaler_mode_enable();
		jpeg_fifo_line_set(C_JPG_FIFO_DISABLE);

		if (jpeg_vlc_addr_set((INT32U) buf)) {
			DBG_PRINT("Calling to jpeg_vlc_addr_set() failed\r\n");
			return -1;
		}
		if (jpeg_multi_vlc_input_init(len)) {
			DBG_PRINT("Calling to jpeg_multi_vlc_input_init() failed\r\n");
			return -1;
		}
		if (jpeg_decompression_start()) {
			DBG_PRINT("Calling to jpeg_decompression_start() failed\r\n");
			return -1;
		}
		mjpeg_decode_started = 1;
	}

	return 0;
}

void mjpeg_decode_stop(void)
{
	scaler_stop();

	jpeg_stop();

	mjpeg_decode_started = 0;
}

// Return the current status of JPEG engine
INT32S mjpeg_decode_status_query(INT32U wait)
{
	return jpeg_status_polling(wait);
}

#endif 		// GPLIB_JPEG_DECODE_EN == 1