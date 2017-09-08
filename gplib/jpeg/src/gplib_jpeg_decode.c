/*
* Purpose: APIs for decompress JPEG image
*
* Author: Tristan Yang
*
* Date: 2007/10/20
*
* Copyright Generalplus Corp. ALL RIGHTS RESERVED.
*
* Version : 1.02
*/

#include "gplib_jpeg_decode.h"


#if (defined GPLIB_JPEG_DECODE_EN) && (GPLIB_JPEG_DECODE_EN == 1)

#define MAX_QUANTIZATION_NUM		4
#define MAX_HUFFMAN_NUM				4		// 0 and 1 are DC tables, 2 and 3 are AC tables
#define MAX_FRAME_COMPONENT_NUM		4
#define MAX_SCAN_COMPONENT_NUM		3

// static global variables for JPEG header parser only
static INT8U *parse_buf;
static INT8U *parse_end;
static PARSE_MAIN_STATE_ENUM main_state;
static PARSE_MAIN_STATE_ENUM saved_main_state;	// For returning from thumbnial parsing
static PARSE_TABLE_STATE_ENUM table_state;
static PARSE_QUANT_STATE_ENUM quantization_state;
static PARSE_HUFFMAN_STATE_ENUM huffman_state;
static PARSE_ARITHMETIC_STATE_ENUM arithmetic_state;
static PARSE_DRI_STATE_ENUM restart_state;
static PARSE_COMMENT_STATE_ENUM comment_state;
static PARSE_APP_STATE_ENUM app_state;
static PARSE_APP_EXIF_STATE_ENUM app_exif_state;
static PARSE_FRAME_STATE_ENUM frame_state;
static PARSE_SCAN_STATE_ENUM scan_state;
static INT32U parsed_count;
static INT16U application_len;
static INT16U thumbnail_application_len;
static INT8U parsing_thumbnail;				// Parsing status: 0=parsing image header, 1=parsing thumbnail header

//static INT8U parse_exif;
static INT8U decode_thumbnail;				// Decode request: 0=decode original image, 1=decode thumbnail

static INT16U image_width;
static INT16U image_height;
static INT16U extend_width;
static INT16U extend_height;
static INT16U yuv_mode;
static INT8U progressive_jpeg;
static INT8U thumbnail_exist;
static INT16U thumbnail_image_width;
static INT16U thumbnail_image_height;
static INT16U thumbnail_extend_width;
static INT16U thumbnail_extend_height;
static INT16U thumbnail_yuv_mode;
static INT8U orientation;
static INT8U date_time[20];
static INT8U *vlc_start;

static INT8U quantization_installed[MAX_QUANTIZATION_NUM];
static INT16U quantization_table[MAX_QUANTIZATION_NUM][64];

static INT8U huffman_installed[MAX_HUFFMAN_NUM];			// 0 and 1 are DC, 2 and 3 are AC
static INT8U huffman_bits[MAX_HUFFMAN_NUM][16];			// 0 and 1 are DC, 2 and 3 are AC
static INT16U huffman_mincode[16];
static INT8U huffman_valptr[16];
static INT8U huffman_dc_values[2][16];
static INT8U huffman_ac_values[2][256];

static INT8U frame_comp_number;
static INT8U thumbnail_frame_comp_number;
static INT8U frame_comp_identifier[MAX_FRAME_COMPONENT_NUM];
static INT8U thumbnail_frame_comp_identifier[MAX_FRAME_COMPONENT_NUM];
static INT8U frame_comp_factor[MAX_FRAME_COMPONENT_NUM];
static INT8U thumbnail_frame_comp_factor[MAX_FRAME_COMPONENT_NUM];		// For thumbnail parsing
static INT8U frame_comp_qtbl_index[MAX_FRAME_COMPONENT_NUM];

static INT8U scan_comp_huffman_index[MAX_SCAN_COMPONENT_NUM];
static INT8U scan_comp_factor[MAX_SCAN_COMPONENT_NUM];
static INT8U scan_comp_qtbl_index[MAX_SCAN_COMPONENT_NUM];

static INT8U jpeg_decode_started = 0;

void jpeg_decode_parse_init(void);
INT32S jpeg_decode_parse_thumbnail_header(INT8U *buf_start);
INT32S parse_table(void);
INT32S parse_quantization(void);
INT32S parse_huffman(void);
INT32S parse_arithmetic(void);
INT32S parse_restart_interval(void);
INT32S parse_comment(void);
INT32S parse_application(void);
INT32S parse_app_exif(void);
INT32S parse_frame(void);
INT32S parse_scan(void);
void calcuate_image_width_height(void);

void jpeg_decode_init(void)
{
	// Initiate header parser information
	decode_thumbnail = 0;
	application_len = 0;
	thumbnail_application_len = 0;
	parsing_thumbnail = 0;
	jpeg_decode_parse_init();

	// Initiate JPEG hardware engine
	jpeg_init();
}

// Initiate header parser information
void jpeg_decode_parse_init(void)
{
	INT32U i;

	// Initiate state machine
	main_state = STATE_SOI;
	table_state = STATE_LARVAL;
	quantization_state = STATE_Q_LEN;
	huffman_state = STATE_H_LEN;
	arithmetic_state = STATE_ARITHMETIC_LEN;
	restart_state = STATE_DRI_LEN;
	comment_state = STATE_COMMENT_LEN;
	app_state = STATE_APP_LEN;
	app_exif_state = STATE_APP_EXIF_IDENTIFIER;
	frame_state = STATE_FRAME_LEN;
	scan_state = STATE_SCAN_LEN;
	parsed_count = 0;

	if (!parsing_thumbnail) {
		image_width = 0;
		image_height = 0;
		extend_width = 0;
		extend_height = 0;
		yuv_mode = 0;
		progressive_jpeg = 0;
	}

	thumbnail_exist = 0;
	thumbnail_image_width = 0;
	thumbnail_image_height = 0;
	thumbnail_extend_width = 0;
	thumbnail_extend_height = 0;
	thumbnail_yuv_mode = 0;
	thumbnail_frame_comp_number = 0;

	if (!parsing_thumbnail) {
		orientation = 1;
		date_time[0] = 0x0;

		vlc_start = NULL;

		frame_comp_number = 0;
		for (i=0; i<MAX_QUANTIZATION_NUM ; i++) {
			quantization_installed[i] = 0x0;
		}
		for (i=0; i<MAX_HUFFMAN_NUM ; i++) {
			huffman_installed[i] = 0x0;
		}
	}

	jpeg_decode_started = 0;
}

// State machine that parses JPEG header
INT32S jpeg_decode_parse_header(INT8U *buf_start, INT32U len)
{
	INT32S status;

	if (!len) {
		DBG_PRINT("jpeg_decode_parse_header(): parameter len is 0!\r\n");
		return JPEG_PARSE_FAIL;
	}
	parse_buf = buf_start;
	parse_end = parse_buf + len;

	while (1) {
		if (parsing_thumbnail) {
			if (main_state == STATE_PARSING_THUMBNAIL) {
				main_state = STATE_SOI;
				buf_start = parse_buf;
			}
			status = jpeg_decode_parse_thumbnail_header(buf_start);
			if (status==JPEG_PARSE_NOT_DONE) {
				return status;
			} else if (status == JPEG_PARSE_DONE) {
				return JPEG_PARSE_OK;
			}
			parsing_thumbnail = 0;

			main_state = saved_main_state;
			table_state = STATE_LARVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

		switch (main_state) {
		case STATE_SOI:
			if (!parsed_count) {
				if (*parse_buf++ != 0xFF) {
					DBG_PRINT("jpeg_decode_parse_header(): First byte is not 0xFF!\r\n");
					return JPEG_PARSE_FAIL;
				}
				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (*parse_buf++ != 0xD8) {			// 0xFFD8 is start of image marker
				DBG_PRINT("jpeg_decode_parse_header(): Second byte is not 0xD8!\r\n");
				return JPEG_PARSE_FAIL;
			}

			main_state = STATE_TABLE1;
			table_state = STATE_LARVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_TABLE1:
			status = parse_table();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (main_state == STATE_PARSING_THUMBNAIL) {
				if (!parsing_thumbnail) {
					DBG_PRINT("jpeg_decode_parse_header(): Not parsing thumbnail but in STATE_PARSING_THUMBNAIL state!\r\n");
					return JPEG_PARSE_FAIL;
				}
				break;
			}
			// Else enter frame header parser state
			main_state = STATE_FRAME;
			frame_state = STATE_FRAME_LEN;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_FRAME:
			status = parse_frame();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			// Else enter table parser state
			main_state = STATE_TABLE2;
			table_state = STATE_LARVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_TABLE2:
			status = parse_table();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (main_state == STATE_PARSING_THUMBNAIL) {
				if (!parsing_thumbnail) {
					DBG_PRINT("jpeg_decode_parse_header(): Not parsing thumbnail but in STATE_PARSING_THUMBNAIL state!\r\n");
					return JPEG_PARSE_FAIL;
				}
				break;
			}
			// Else enter scan header parser state
			main_state = STATE_SCAN;
			scan_state = STATE_SCAN_LEN;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_SCAN:
			status = parse_scan();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			main_state = STATE_VLC;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_VLC:
			vlc_start = parse_buf;
			calcuate_image_width_height();

			return JPEG_PARSE_OK;

		default:
			DBG_PRINT("jpeg_decode_parse_header(): Unknown state!\r\n");
			return JPEG_PARSE_FAIL;
		}
	}
	return JPEG_PARSE_FAIL;
}

// State machine that parses JPEG header
INT32S jpeg_decode_parse_thumbnail_header(INT8U *buf_start)
{
	static INT32S status;

	while (1) {
		switch (main_state) {
		case STATE_SOI:
			if (!parsed_count) {
				if (*parse_buf++ != 0xFF) {
					DBG_PRINT("jpeg_decode_parse_thumbnail_header(): First byte is not 0xFF!\r\n");

					status = JPEG_PARSE_FAIL;
					main_state = STATE_SKIP_THUMBNAIL;
					break;
				}
				parsed_count++;
				if (parse_buf >= parse_end) {
					application_len -= parse_buf - buf_start;
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (*parse_buf++ != 0xD8) {			// 0xFFD8 is start of image marker
				DBG_PRINT("jpeg_decode_parse_thumbnail_header(): Second byte is not 0xD8!\r\n");
				status = JPEG_PARSE_FAIL;
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}

			main_state = STATE_TABLE1;
			table_state = STATE_LARVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_TABLE1:
			status = parse_table();
			if (status == JPEG_PARSE_NOT_DONE) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
			if (status != JPEG_PARSE_OK) {
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}
			// Else enter frame header parser state
			main_state = STATE_FRAME;
			frame_state = STATE_FRAME_LEN;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_FRAME:
			status = parse_frame();
			if (status == JPEG_PARSE_NOT_DONE) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
			if (status != JPEG_PARSE_OK) {
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}
			// Else enter table parser state
			main_state = STATE_TABLE2;
			table_state = STATE_LARVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_TABLE2:
			status = parse_table();
			if (status == JPEG_PARSE_NOT_DONE) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
			if (status != JPEG_PARSE_OK) {
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}
			// Else enter scan header parser state
			main_state = STATE_SCAN;
			scan_state = STATE_SCAN_LEN;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_SCAN:
			status = parse_scan();
			if (status == JPEG_PARSE_NOT_DONE) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
			if (status != JPEG_PARSE_OK) {
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}
			main_state = STATE_VLC;
			if (parse_buf >= parse_end) {
				application_len -= parse_buf - buf_start;
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_VLC:
			thumbnail_exist = 1;
			calcuate_image_width_height();

			if (!decode_thumbnail) {
				// Skip thumbnail data and back to original image
				status = JPEG_PARSE_THUMBNAIL_DONE;
				main_state = STATE_SKIP_THUMBNAIL;
				break;
			}

			vlc_start = parse_buf;
			application_len -= parse_buf - buf_start;

			return JPEG_PARSE_DONE;

		case STATE_SKIP_THUMBNAIL:
			// Skip application data that is not handled
			if (application_len > (parse_end - buf_start)) {
				application_len -= parse_end - buf_start;
				parse_buf = parse_end;
				return JPEG_PARSE_NOT_DONE;
			}
			if (application_len < parse_buf - parse_end) {
				DBG_PRINT("jpeg_decode_parse_thumbnail_header(): Run out of application data!\r\n");
				return JPEG_PARSE_FAIL;
			}
			parse_buf += application_len - (parse_buf - buf_start);
			application_len = 0;

			return status;

		default:
			DBG_PRINT("jpeg_decode_parse_thumbnail_header(): Unknown state!\r\n");
			status = JPEG_PARSE_FAIL;
			main_state = STATE_SKIP_THUMBNAIL;
			break;
		}
	}

	return JPEG_PARSE_FAIL;
}

INT32S parse_table(void)
{
	INT8U temp_char;
	INT32S status;

	while (1) {
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

		switch (table_state) {
		case STATE_LARVAL:
			if (!parsed_count) {
				if (*parse_buf++ != 0xFF) {			// Find next marker
					continue;
				}
				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}

			temp_char = *parse_buf++;
			if (temp_char == 0xC0) {				// 0xFFC0 is start of frame marker of baseline JPEG
				return JPEG_PARSE_OK;
			} else if (temp_char == 0xC1) {			// 0xFFC1 is start of frame marker of extended JPEG
				return JPEG_PARSE_OK;
			} else if (temp_char == 0xC2) {			// 0xFFC2 is Progressive JPEG
				if (!parsing_thumbnail) {
					progressive_jpeg = 1;
				} else {
					DBG_PRINT("parse_table(): progressive mode in thumbnail is not supported!\r\n");
					return JPEG_PARSE_FAIL;
				}
				return JPEG_PARSE_OK;
			} else if (temp_char == 0xC4) {			// 0xFFC4 is define huffman table marker
				table_state = STATE_HUFFMAN_TABLE;
				huffman_state = STATE_H_LEN;
			} else if (temp_char == 0xCC) {			// 0xFFCC is define arithemtic table marker
				table_state = STATE_ARITHMETIC;
				arithmetic_state = STATE_ARITHMETIC_LEN;
			} else if (temp_char == 0xDA) {			// 0xFFDA is start of scan marker
				return JPEG_PARSE_OK;
			} else if (temp_char == 0xDB) {			// 0xFFDB is define quantization table marker
				table_state = STATE_QUANTIZATION_TABLE;
				quantization_state = STATE_Q_LEN;
			} else if (temp_char == 0xDD) {			// 0xFFDD is define restart interval marker
				table_state = STATE_DEFINE_RESTART;
				restart_state = STATE_DRI_LEN;
			} else if (temp_char>=0xE0 && temp_char<=0xEF) {		// 0xFFE0 ~ 0xFFEF are application markers
				table_state = STATE_APP;
				app_state = STATE_APP_LEN;
			} else if (temp_char == 0xFE) {			// 0xFFFE is comment marker
				table_state = STATE_COMMENT;
				comment_state = STATE_COMMENT_LEN;
			} else if (temp_char == 0x00) {			// Just skip it
			} else if (temp_char == 0xFF) {
				parsed_count = 1;
				continue;
			} else {
				DBG_PRINT("parse_table(): Second byte(0x%02X) of marker code is not supported!\r\n", temp_char);
				return JPEG_PARSE_FAIL;
			}

			if (parse_buf >= parse_end) {
				parsed_count = 0;
				return JPEG_PARSE_NOT_DONE;
			}
			break;
		case STATE_QUANTIZATION_TABLE:
			status = parse_quantization();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		case STATE_HUFFMAN_TABLE:
			status = parse_huffman();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		case STATE_ARITHMETIC:
			status = parse_arithmetic();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		case STATE_DEFINE_RESTART:
			status = parse_restart_interval();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		case STATE_COMMENT:
			status = parse_comment();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		case STATE_APP:
			status = parse_application();
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (main_state == STATE_PARSING_THUMBNAIL) {
				return status;
			}
			table_state = STATE_LARVAL;
			break;
		default:
			DBG_PRINT("parse_table(): Unknown state!\r\n");
			return JPEG_PARSE_FAIL;
		}

		parsed_count = 0;
	}
	return JPEG_PARSE_OK;
}

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
INT32S parse_quantization(void)
{
	static INT16U quant_len;
	static INT8U identifier;

	while (1) {
		switch (quantization_state) {
		case STATE_Q_LEN:
			if (!parsed_count) {
				quant_len = ((INT16U) *parse_buf++) << 8;

				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			quant_len |= (INT16U) (*parse_buf++);
			quant_len -= 2;
			if (quant_len < 65) {
				DBG_PRINT("parse_quantization(): table len(%d) is less than 65!\r\n", quant_len);
				return JPEG_PARSE_FAIL;
			}

			quantization_state = STATE_Q_IDENTIFIER;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_Q_IDENTIFIER:
			identifier = *parse_buf++;
			if (identifier > 0x13) {
				DBG_PRINT("parse_quantization(): Pq and Tq(0x%x) is larger than 0x13!\r\n", identifier);
			} else if (identifier & 0x10) {			// 16 bit precision
				if (quant_len < 129) {
					DBG_PRINT("parse_quantization(): table len(%d) is less than 129!\r\n", quant_len);
					return JPEG_PARSE_FAIL;
				}
			}
			if ((identifier&0xF) >= MAX_QUANTIZATION_NUM) {
				DBG_PRINT("parse_quantization(): Ideqnitifer(0x%x) is larger than %d!\r\n", identifier&0x0F, MAX_QUANTIZATION_NUM-1);
				return JPEG_PARSE_FAIL;
			}
			if (!(identifier&0xF0)) {
				if (!parsing_thumbnail || decode_thumbnail) {
					quantization_installed[identifier&0xF] = 1;
				}
			}

			quant_len--;
			quantization_state = STATE_ELEMENT;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_ELEMENT:
			if (!(identifier&0xF0)) {	// 8-bit precision
				while (parsed_count < 64) {
					if (!parsing_thumbnail || decode_thumbnail) {
						quantization_table[identifier&0xF][zigzag_scan[parsed_count]] = (INT16U) (*parse_buf);
					}
					parse_buf++;
					parsed_count++;

					if (!(--quant_len) && parsed_count!=64) {
						DBG_PRINT("parse_quantization(): less than 64 entries(%d) are read!\r\n", parsed_count);
						return JPEG_PARSE_FAIL;
					}
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			} else {					// 16-bit precision
				while (quant_len) {
					quant_len--;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			}

			// Check whether we are done here
			if (quant_len) {
				quantization_state = STATE_Q_IDENTIFIER;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
				break;
			}
			return JPEG_PARSE_OK;
		}		// switch
	}		// while (1)

	return JPEG_PARSE_OK;
}

INT8U huffman_temp[256];

INT32S huffman_gen_table(INT8U luminance, INT8U dc, INT8U selector)
{
	INT16U  i, j, last_k;
	INT16U  k, code;
	INT16U  count;
	INT8U   si;

	if (dc) {								// DC
		count = 16;
	} else {
		count = 256;
	}
	// Generation of table of Huffman code sizes, ISO/IEC 10918-1:1994(E) page 51
	last_k = 0;
	i = 1;
	j = 1;
	do {
		if (j > huffman_bits[selector][i-1]) {		// BITS
			i++;
			j = 1;
			if (i > 16) {
				for (i=last_k; i<count; i++) {
					huffman_temp[i] = 0;	// HUFFSIZE
				}
				break;
			}
		} else {
			if (last_k >= 256) {
				return JPEG_PARSE_FAIL;
			}
			huffman_temp[last_k] = i;			// HUFFSIZE
			last_k++;
			j++;
		}
	} while (1);

	// Generation of table of Huffman codes,  ISO/IEC 10918-1:1994(E) page 52
	k = 0;
	code = 0;
	si = huffman_temp[0];					// HUFFSIZE
	for (j=0; (j<si) && (j<16); j++) {
		huffman_mincode[j] = code;
		huffman_valptr[j] = k;
	}
	do {
		do {
			code++;
			k++;
			if (k >=256) {
				return JPEG_PARSE_FAIL;
			}
		} while (huffman_temp[k] == si);	// HUFFSIZE

		if (huffman_temp[k] == 0) {			// HUFFSIZE
			while (si < 16) {
				code <<= 1;
				huffman_mincode[si] = code;
				huffman_valptr[si] = k;
				si++;
			}
			break;
		}

		do {
			code <<= 1;
			if (si >= 16) {
				return JPEG_PARSE_FAIL;
			}
			huffman_mincode[si] = code;
			huffman_valptr[si] = k;
			si++;
		} while (huffman_temp[k] > si);	// HUFFSIZE

	} while (1);

	if (dc) {								// DC
		if (luminance) {
			if (jpeg_huffman_dc_lum_mincode_set(0, 16, &huffman_mincode[0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_lum_mincode_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
			if (jpeg_huffman_dc_lum_valptr_set(0, 16, &huffman_valptr[0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_lum_valptr_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		} else {
			if (jpeg_huffman_dc_chrom_mincode_set(0, 16, &huffman_mincode[0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_chrom_mincode_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
			if (jpeg_huffman_dc_chrom_valptr_set(0, 16, &huffman_valptr[0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_chrom_valptr_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
	} else {								// AC
		if (luminance) {
			if (jpeg_huffman_ac_lum_mincode_set(0, 16, &huffman_mincode[0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_lum_mincode_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
			if (jpeg_huffman_ac_lum_valptr_set(0, 16, &huffman_valptr[0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_lum_valptr_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		} else {
			if (jpeg_huffman_ac_chrom_mincode_set(0, 16, &huffman_mincode[0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_chrom_mincode_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
			if (jpeg_huffman_ac_chrom_valptr_set(0, 16, &huffman_valptr[0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_chrom_valptr_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
	}

	return JPEG_PARSE_OK;
}

INT32S parse_huffman(void)
{
	static INT16U huffman_len;
	static INT8U identifier, index;
	static INT8U huffman_count;

	while (1) {
		switch (huffman_state) {
		case STATE_H_LEN:
			if (!parsed_count) {
				huffman_len = ((INT16U) *parse_buf++) << 8;

				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			huffman_len |= (INT16U) (*parse_buf++);
			huffman_len -= 2;

			huffman_state = STATE_H_IDENTIFIER;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_H_IDENTIFIER:
			identifier = *parse_buf++;
			if (identifier & 0xEC) {
				DBG_PRINT("parse_huffman(): Unsupported identifer(0x%02X)!\r\n", identifier);
				return JPEG_PARSE_FAIL;
			}

			huffman_state = STATE_NUMBER_CODELEN;
			huffman_len--;
			parsed_count = 0;
			huffman_count = 0;
			index = ((identifier>>3) & 0x2) | (identifier & 0x1);		// 0 and 1 are DC, 2 and 3 are AC
			if (!parsing_thumbnail || decode_thumbnail) {
				huffman_installed[index] = 1;
			}
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_NUMBER_CODELEN:
			// Get number of Huffman codes of length 1 ~ 16 bits
			while (parsed_count < 16) {
				if (!parsing_thumbnail || decode_thumbnail) {
					huffman_bits[index][parsed_count] = *parse_buf;
				}
				huffman_count += *parse_buf++;
				parsed_count++;

				if (!(--huffman_len) && parsed_count!=16) {
					DBG_PRINT("parse_huffman(): Less than 16 entries(%d) are read!\r\n", parsed_count);
					return JPEG_PARSE_FAIL;
				}

				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (identifier & 0x10) {		// AC
				if (huffman_count > 256) {
					DBG_PRINT("parse_huffman(): Number(%d) of huffman AC code values is larger than 256!\r\n", huffman_count);
					return JPEG_PARSE_FAIL;
				}
			} else {						// DC
				if (huffman_count > 16) {
					DBG_PRINT("parse_huffman(): Number(%d) of huffman DC code values is larger than 256!\r\n", huffman_count);
					return JPEG_PARSE_FAIL;
				}
			}

			huffman_state = STATE_HUFFVAL;
			parsed_count = 0;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		case STATE_HUFFVAL:
			// Get value associated with each Huffman code
			while (parsed_count < huffman_count) {
				if (!parsing_thumbnail || decode_thumbnail) {
					if (identifier & 0x10) {	// AC
						huffman_ac_values[identifier&0x1][parsed_count] = *parse_buf;
					} else {					// DC
						huffman_dc_values[identifier&0x1][parsed_count] = *parse_buf;
					}
				}
				parse_buf++;

				huffman_len--;
				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}

			if (!parsing_thumbnail || decode_thumbnail) {
				if (identifier & 0x10) {		// AC
					while (parsed_count < 256) {
						huffman_ac_values[identifier&0x1][parsed_count++] = 0x0;
					}
				} else {						// DC
					while (parsed_count < 16) {
						huffman_dc_values[identifier&0x1][parsed_count++] = 0x0;
					}
				}
			}

			// Check if we are done here
			if (huffman_len) {
				huffman_state = STATE_H_IDENTIFIER;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
				break;
			}
			return JPEG_PARSE_OK;
		}		// switch
	}		// while (1)

	return JPEG_PARSE_OK;
}

INT32S parse_arithmetic(void)
{
	static INT16U arithmetic_len;

	switch (arithmetic_state) {
	case STATE_ARITHMETIC_LEN:
		if (!parsed_count) {
			arithmetic_len = ((INT16U) *parse_buf++) << 8;

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		arithmetic_len |= (INT16U) (*parse_buf++);
		arithmetic_len -= 2;

		arithmetic_state = STATE_COMMENT_DATA;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_ARITHMETIC_DATA:
		// Skip arithmetic data
		if (arithmetic_len > (parse_end - parse_buf)) {
			arithmetic_len -= parse_end - parse_buf;
			parse_buf = parse_end;
			return JPEG_PARSE_NOT_DONE;
		}
		parse_buf += arithmetic_len;
	}
	return JPEG_PARSE_OK;
}

INT32S parse_restart_interval(void)
{
	static INT16U restart_interval;

	if (!parsed_count) {
		if (*parse_buf++ != 0x0) {
			DBG_PRINT("parse_restart_interval(): First byte(0x%02X) is not 0x00!\r\n", *(parse_buf-1));
			return JPEG_PARSE_FAIL;
		}

		parsed_count++;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	}
	if (parsed_count == 1) {
		if (*parse_buf++ != 0x4) {
			DBG_PRINT("parse_restart_interval(): length(%d) is not 0x04!\r\n", *(parse_buf-1));
			return JPEG_PARSE_FAIL;
		}

		parsed_count++;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	}
	if (parsed_count == 2) {
		restart_interval = ((INT16U) *parse_buf++) << 8;

		parsed_count++;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	}

	restart_interval |= (INT16U) *parse_buf++;
	if (!parsing_thumbnail || decode_thumbnail) {
		if (jpeg_restart_interval_set(restart_interval)) {
			DBG_PRINT("Calling to jpeg_restart_interval_set() failed\r\n");
			return JPEG_SET_HW_ERROR;
		}
	}

	return JPEG_PARSE_OK;
}

INT32S parse_comment(void)
{
	static INT16U comment_len;

	switch (comment_state) {
	case STATE_COMMENT_LEN:
		if (!parsed_count) {
			comment_len = ((INT16U) *parse_buf++) << 8;

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		comment_len |= (INT16U) (*parse_buf++);
		comment_len -= 2;

		comment_state = STATE_COMMENT_DATA;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_COMMENT_DATA:
		// Skip unknown application data
		if (comment_len > (parse_end - parse_buf)) {
			comment_len -= parse_end - parse_buf;
			parse_buf = parse_end;
			return JPEG_PARSE_NOT_DONE;
		}
		parse_buf += comment_len;
	}
	return JPEG_PARSE_OK;
}

INT32S parse_application(void)
{
	INT32S status;
	static INT8U app_name[6];

	switch (app_state) {
	case STATE_APP_LEN:
		if (!parsed_count) {
			if (!parsing_thumbnail) {
				application_len = ((INT16U) *parse_buf) << 8;
			} else {
				thumbnail_application_len =  ((INT16U) *parse_buf) << 8;
			}
			parse_buf++;

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		if (!parsing_thumbnail) {
			application_len |= (INT16U) (*parse_buf);
			application_len -= 2;
		} else {
			thumbnail_application_len |= (INT16U) (*parse_buf);
			thumbnail_application_len -= 2;
		}
		parse_buf++;

		if (parsing_thumbnail) {
			app_state = STATE_APP_SKIP_DATA;		// Skip all application data in thumbnail image
		} else if (application_len > 4) {
			app_state = STATE_APP_IDENTIFIER;
			parsed_count = 0;
			app_name[0] = 0x0;
		} else if (!application_len) {
			return JPEG_PARSE_OK;
		} else {
			app_state = STATE_APP_SKIP_DATA;		// Try to skip operations in STATE_APP_IDENTIFIER
		}
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_APP_IDENTIFIER:
		if (app_state == STATE_APP_IDENTIFIER) {
			while (parsed_count < 5) {
				app_name[parsed_count] = *parse_buf++;
				parsed_count++;

				application_len--;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (app_name[0]=='J' && app_name[1]=='F' && app_name[2]=='I' && app_name[3]=='F' && app_name[4]==0x00) {
				app_state = STATE_APP_JFIF;
			} else if (app_name[0]=='E' && app_name[1]=='x' && app_name[2]=='i' && app_name[3]=='f' && app_name[4]==0x00) {
				app_state = STATE_APP_EXIF;
				app_exif_state = STATE_APP_EXIF_IDENTIFIER;
			} else {
				app_state = STATE_APP_SKIP_DATA;
			}
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
	case STATE_APP_JFIF:
		if (app_state == STATE_APP_JFIF) {
			app_state = STATE_APP_SKIP_DATA;
		}
	case STATE_APP_EXIF:
		if (app_state == STATE_APP_EXIF) {
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
			status = parse_app_exif();
			if (status == JPEG_PARSE_NOT_DONE) {
				return status;
			}
			if (main_state == STATE_PARSING_THUMBNAIL) {
				return status;
			}
			app_state = STATE_APP_SKIP_DATA;
		}
	case STATE_APP_SKIP_DATA:
		// Skip unknown application data
		if (!parsing_thumbnail) {
			if (application_len > (parse_end - parse_buf)) {
				application_len -= parse_end - parse_buf;
				parse_buf = parse_end;
				return JPEG_PARSE_NOT_DONE;
			}

			parse_buf += application_len;
			application_len = 0;
		} else {
			if (thumbnail_application_len > (parse_end - parse_buf)) {
				thumbnail_application_len -= parse_end - parse_buf;
				parse_buf = parse_end;
				return JPEG_PARSE_NOT_DONE;
			}

			parse_buf += thumbnail_application_len;
			thumbnail_application_len = 0;
		}
	}
	return JPEG_PARSE_OK;
}

INT32S parse_app_exif(void)
{
	static INT32U current_tiff_offset, target_tiff_offset, date_time_offset;
	static INT16U field_number;
	static INT16U tag, type;
	static INT32U count, value;
	static INT8U byte_order;						// 0=little endian, 1=big endian

	switch (app_exif_state) {
	case STATE_APP_EXIF_IDENTIFIER:
		// One more byte(normally 0x00) to skip
		parse_buf++;
		app_exif_state = STATE_APP_EXIF_TIFF_BYTE_ORDER;
		application_len -= 1;
		parsed_count = 0;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_TIFF_BYTE_ORDER:
		// Parse TIFF header: byte order
		if (!parsed_count) {
			if (*parse_buf == 0x49) {		// Little endian
				byte_order = 0;
			} else if (*parse_buf == 0x4D) {		// Big endian
				byte_order = 1;
			} else {
				DBG_PRINT("parse_app_exif() Unknown byte order(0x%02x)\r\n",  *parse_buf);
				return JPEG_PARSE_FAIL;
			}
			application_len -= 1;
			parsed_count++;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

		// Second byte of byte-order must be the same as the first byte
		if (!byte_order) {					// Little endian
			if (*parse_buf != 0x49) {
				DBG_PRINT("parse_app_exif() second byte order is not 0x49(0x%02x)\r\n",  *parse_buf);
				return JPEG_PARSE_FAIL;
			}
		} else {							// Big endian
			if (*parse_buf != 0x4D) {
				DBG_PRINT("parse_app_exif() second byte order is not 0x4D(0x%02x)\r\n",  *parse_buf);
				return JPEG_PARSE_FAIL;
			}
		}
		app_exif_state = STATE_APP_EXIF_TIFF_42;
		application_len -= 1;
		parsed_count = 0;
		parse_buf++;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_TIFF_42:
		// Parse TIFF header: 0x002A
		if (!parsed_count) {
			if (!byte_order) {					// Little endian
				if (*parse_buf != 0x2A) {
					DBG_PRINT("parse_app_exif() can not find 0x2A(0x%02x)\r\n",  *parse_buf);
					return JPEG_PARSE_FAIL;
				}
			} else {							// Big endian
				if (*parse_buf != 0x00) {
					DBG_PRINT("parse_app_exif() can not find 0x00(0x%02x)\r\n",  *parse_buf);
					return JPEG_PARSE_FAIL;
				}
			}
			application_len -= 1;
			parsed_count++;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

		if (!byte_order) {					// Little endian
			if (*parse_buf != 0x00) {
				DBG_PRINT("parse_app_exif() can not find 0x00(0x%02x)\r\n",  *parse_buf);
				return JPEG_PARSE_FAIL;
			}
		} else {							// Big endian
			if (*parse_buf != 0x2A) {
				DBG_PRINT("parse_app_exif() can not find 0x2A(0x%02x)\r\n",  *parse_buf);
				return JPEG_PARSE_FAIL;
			}
		}
		app_exif_state = STATE_APP_EXIF_TIFF_IFD0;
		application_len -= 1;
		parsed_count = 0;
		parse_buf++;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_TIFF_IFD0:
		// Parse TIFF header: offset of IFD from TIFF header
		if (!byte_order) {					// Little endian
			if (!parsed_count) {
				target_tiff_offset = *parse_buf;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 1) {
				target_tiff_offset |= (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 2) {
				target_tiff_offset |= (*parse_buf)<<16;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			target_tiff_offset |= (*parse_buf)<<24;
			current_tiff_offset = 8;
			if (target_tiff_offset < current_tiff_offset) {
				if (target_tiff_offset) {
					DBG_PRINT("parse_app_exif() IFD0 offset is incorrect(0x%x < 0x%x)\r\n",  target_tiff_offset, current_tiff_offset);
				}
				return JPEG_PARSE_FAIL;
			}

			app_exif_state = STATE_APP_EXIF_JUMP_IFD0;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		} else {							// Big endian
			if (!parsed_count) {
				target_tiff_offset = (*parse_buf)<<24;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 1) {
				target_tiff_offset |= (*parse_buf)<<16;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 2) {
				target_tiff_offset |= (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			target_tiff_offset |= (*parse_buf);
			current_tiff_offset = 8;
			if (target_tiff_offset < current_tiff_offset) {
				if (target_tiff_offset) {
					DBG_PRINT("parse_app_exif() IFD0 offset is incorrect(0x%x < 0x%x)\r\n",  target_tiff_offset, current_tiff_offset);
				}
				return JPEG_PARSE_FAIL;
			}

			app_exif_state = STATE_APP_EXIF_JUMP_IFD0;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

	case STATE_APP_EXIF_JUMP_IFD0:
		if (target_tiff_offset-current_tiff_offset > (parse_end - parse_buf)) {
			if (application_len < parse_end - parse_buf) {
				DBG_PRINT("parse_app_exif() run out of application data\r\n");
				return JPEG_PARSE_FAIL;
			}
			application_len -= parse_end - parse_buf;
			current_tiff_offset += parse_end - parse_buf;
			parse_buf = parse_end;
			return JPEG_PARSE_NOT_DONE;
		}
		app_exif_state = STATE_APP_EXIF_IFD0_FIELD_NUM;
		if (application_len < target_tiff_offset-current_tiff_offset) {
			DBG_PRINT("parse_app_exif() run out of application data\r\n");
			return JPEG_PARSE_FAIL;
		}
		application_len -= target_tiff_offset-current_tiff_offset;
		parsed_count = 0;
		parse_buf += target_tiff_offset-current_tiff_offset;
		current_tiff_offset = target_tiff_offset;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_IFD0_FIELD_NUM:
		if (!byte_order) {					// Little endian
			if (!parsed_count) {
				field_number = *parse_buf;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			field_number |= (*parse_buf)<<8;
			app_exif_state = STATE_APP_EXIF_IFD0_FIELD;
			date_time_offset = 0;
			current_tiff_offset += 2;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		} else {							// Big endian
			if (!parsed_count) {
				field_number = (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			field_number |= (*parse_buf);
			app_exif_state = STATE_APP_EXIF_IFD0_FIELD;
			date_time_offset = 0;
			current_tiff_offset += 2;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

	case STATE_APP_EXIF_IFD0_FIELD:
		while (field_number) {
			// Tag(2 bytes) + Type(2 bytes) + Count(4 bytes) + Value(4 bytes)
			if (application_len < 12) {
				DBG_PRINT("parse_app_exif() run out of application data\r\n");
				return JPEG_PARSE_FAIL;
			}
			if (!byte_order) {					// Little endian
				if (!parsed_count) {
					tag = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 1) {
					tag |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 2) {
					type = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 3) {
					type |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 4) {
					count = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 5) {
					count |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 6) {
					count |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 7) {
					count |= (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 8) {
					value = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 9) {
					value |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 10) {
					value |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 11) {
					value |= (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;

					application_len -= 12;
					current_tiff_offset += 12;
					parsed_count = 0;
					field_number--;
					if (tag == 0x9003) {			// Date and time original image was generated
						date_time_offset = value;
					} else if (tag == 0x0202) {		// Date and time image was made digital data
						date_time_offset = value;
					} else if (tag == 0x0112) {		// Orientation
						orientation = (INT8U) value;
					} else if (tag == 0x0132) {		// Date and time
						if (!date_time_offset) {
							date_time_offset = value;
							if (date_time_offset < current_tiff_offset) {
								date_time_offset = 0;
							}
						}
					}

					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			} else {							// Big endian
				if (!parsed_count) {
					tag = (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 1) {
					tag |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 2) {
					type = (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 3) {
					type |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 4) {
					count = (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 5) {
					count |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 6) {
					count |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 7) {
					count |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 8) {
					value = (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 9) {
					value |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 10) {
					value |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 11) {
					value |= (*parse_buf);
					parsed_count++;
					parse_buf++;

					application_len -= 12;
					current_tiff_offset += 12;
					parsed_count = 0;
					field_number--;
					if (tag == 0x9003) {			// Date and time original image was generated
						date_time_offset = value;
					} else if (tag == 0x0202) {		// Date and time image was made digital data
						date_time_offset = value;
					} else if (tag == 0x0112) {		// Orientation
						orientation = (INT8U) (value >> 16);
					} else if (tag == 0x0132) {		// Date and time
						if (!date_time_offset) {
							date_time_offset = value;
							if (date_time_offset < current_tiff_offset) {
								date_time_offset = 0;
							}
						}
					}

					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			}
		}
		app_exif_state = STATE_APP_EXIF_IFD0_NEXT;
		parsed_count = 0;

	case STATE_APP_EXIF_IFD0_NEXT:
		// Get offset of IDF1
		if (!byte_order) {					// Little endian
			if (!parsed_count) {
				target_tiff_offset = *parse_buf;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 1) {
				target_tiff_offset |= (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 2) {
				target_tiff_offset |= (*parse_buf)<<16;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			target_tiff_offset |= (*parse_buf)<<24;
			current_tiff_offset += 4;
			if (target_tiff_offset < current_tiff_offset) {
				if (target_tiff_offset) {
					DBG_PRINT("parse_app_exif() IFD1 offset is incorrect(0x%x < 0x%x)\r\n",  target_tiff_offset, current_tiff_offset);
					return JPEG_PARSE_FAIL;
				}
				return JPEG_PARSE_OK;
			}

			app_exif_state = STATE_APP_EXIF_JUMP_DATE_TIME1;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		} else {							// Big endian
			if (!parsed_count) {
				target_tiff_offset = (*parse_buf)<<24;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 1) {
				target_tiff_offset |= (*parse_buf)<<16;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 2) {
				target_tiff_offset |= (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			target_tiff_offset |= (*parse_buf);
			current_tiff_offset += 4;
			if (target_tiff_offset < current_tiff_offset) {
				if (target_tiff_offset) {
					DBG_PRINT("parse_app_exif() IFD1 offset is incorrect(0x%x < 0x%x)\r\n",  target_tiff_offset, current_tiff_offset);
					return JPEG_PARSE_FAIL;
				}
				return JPEG_PARSE_OK;
			}

			app_exif_state = STATE_APP_EXIF_JUMP_DATE_TIME1;
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

	case STATE_APP_EXIF_JUMP_DATE_TIME1:
		if (date_time_offset && date_time_offset<target_tiff_offset) {
			if (date_time_offset > current_tiff_offset) {
				if (date_time_offset-current_tiff_offset > (parse_end - parse_buf)) {
					if (application_len < parse_end - parse_buf) {
						DBG_PRINT("parse_app_exif() run out of application data\r\n");
						return JPEG_PARSE_FAIL;
					}
					application_len -= parse_end - parse_buf;
					current_tiff_offset += parse_end - parse_buf;
					parse_buf = parse_end;
					return JPEG_PARSE_NOT_DONE;
				}
				if (application_len < date_time_offset-current_tiff_offset) {
					DBG_PRINT("parse_app_exif() run out of application data\r\n");
					return JPEG_PARSE_FAIL;
				}
				application_len -= date_time_offset - current_tiff_offset;
				parse_buf += date_time_offset - current_tiff_offset;
				current_tiff_offset = date_time_offset;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			// Get Date and time here
			while (parsed_count < 20) {
				if (parsed_count==0 && (*parse_buf<'0' || *parse_buf>'9')) {
					break;
				}
				date_time[parsed_count++] = *parse_buf++;
				current_tiff_offset++;
				application_len--;

				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			date_time_offset = 0;
		}
		app_exif_state = STATE_APP_EXIF_JUMP_IFD1;
		parsed_count = 0;

	case STATE_APP_EXIF_JUMP_IFD1:
		if (target_tiff_offset-current_tiff_offset > (parse_end - parse_buf)) {
			if (application_len < parse_end - parse_buf) {
				DBG_PRINT("parse_app_exif() run out of application data\r\n");
				return JPEG_PARSE_FAIL;
			}
			application_len -= parse_end - parse_buf;
			current_tiff_offset += parse_end - parse_buf;
			parse_buf = parse_end;
			return JPEG_PARSE_NOT_DONE;
		}
		app_exif_state = STATE_APP_EXIF_IFD1_FIELD_NUM;
		if (application_len < target_tiff_offset-current_tiff_offset) {
			DBG_PRINT("parse_app_exif() run out of application data\r\n");
			return JPEG_PARSE_FAIL;
		}
		application_len -= target_tiff_offset-current_tiff_offset;
		parsed_count = 0;
		parse_buf += target_tiff_offset-current_tiff_offset;
		current_tiff_offset = target_tiff_offset;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_IFD1_FIELD_NUM:
		if (!byte_order) {					// Little endian
			if (!parsed_count) {
				field_number = *parse_buf;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			field_number |= (*parse_buf)<<8;
			app_exif_state = STATE_APP_EXIF_IFD1_FIELD;
			current_tiff_offset += 2;
			target_tiff_offset = 0;			// Initiate to 0
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		} else {							// Big endian
			if (!parsed_count) {
				field_number = (*parse_buf)<<8;
				application_len -= 1;
				parsed_count++;
				parse_buf++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			field_number |= (*parse_buf);
			app_exif_state = STATE_APP_EXIF_IFD1_FIELD;
			current_tiff_offset += 2;
			target_tiff_offset = 0;			// Initiate to 0
			application_len -= 1;
			parsed_count = 0;
			parse_buf++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}

	case STATE_APP_EXIF_IFD1_FIELD:
		while (field_number) {
			// Tag(2 bytes) + Type(2 bytes) + Count(4 bytes) + Value(4 bytes)
			if (application_len < 12) {
				DBG_PRINT("parse_app_exif() run out of application data\r\n");
				return JPEG_PARSE_FAIL;
			}
			if (!byte_order) {					// Little endian
				if (!parsed_count) {
					tag = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 1) {
					tag |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 2) {
					type = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 3) {
					type |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 4) {
					count = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 5) {
					count |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 6) {
					count |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 7) {
					count |= (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 8) {
					value = (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 9) {
					value |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 10) {
					value |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 11) {
					value |= (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;

					application_len -= 12;
					current_tiff_offset += 12;
					parsed_count = 0;
					field_number--;
					if (tag == 0x0201) {			// Offset to thumbnail data
						// Ignore type and count checking, some Exif don't follow the specification
						target_tiff_offset = value;
					} else if (tag == 0x0202) {		// Thumbnail data size
						if (value == 0) {	// No thumbnail exists
							//DBG_PRINT("parse_app_exif() thumbnail size is 0\r\n");
							return JPEG_PARSE_FAIL;
						}
					} else if (tag == 0x0112) {		// Orientation
						orientation = (INT8U) value;
					} else if (tag == 0x0132) {		// Date and time
						if (!date_time[0]) {
							date_time_offset = value;
							if (date_time_offset < current_tiff_offset) {
								date_time_offset = 0;
							}
						}
					}

					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			} else {							// Big endian
				if (!parsed_count) {
					tag = (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 1) {
					tag |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 2) {
					type = (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 3) {
					type |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 4) {
					count = (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 5) {
					count |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 6) {
					count |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 7) {
					count |= (*parse_buf);
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 8) {
					value = (*parse_buf) << 24;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 9) {
					value |= (*parse_buf) << 16;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 10) {
					value |= (*parse_buf) << 8;
					parsed_count++;
					parse_buf++;
					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				} else if (parsed_count == 11) {
					value |= (*parse_buf);
					parsed_count++;
					parse_buf++;

					application_len -= 12;
					current_tiff_offset += 12;
					parsed_count = 0;
					field_number--;
					if (tag == 0x0201) {			// Offset to thumbnail data
						// Ignore type and count checking, some Exif don't followed the specification
						target_tiff_offset = value;
					} else if (tag == 0x0202) {		// Thumbnail data size
						if (value == 0) {	// No thumbnail exists
							//DBG_PRINT("parse_app_exif() thumbnail size is 0\r\n");
							return JPEG_PARSE_FAIL;
						}
					} else if (tag == 0x0112) {		// Orientation
						orientation = (INT8U) (value >> 16);
					} else if (tag == 0x0132) {		// Date and time
						if (!date_time[0]) {
							date_time_offset = value;
							if (date_time_offset < current_tiff_offset) {
								date_time_offset = 0;
							}
						}
					}

					if (parse_buf >= parse_end) {
						return JPEG_PARSE_NOT_DONE;
					}
				}
			}
		}
		app_exif_state = STATE_APP_EXIF_JUMP_DATE_TIME2;
		parsed_count = 0;
		if (target_tiff_offset < current_tiff_offset) {
			if (target_tiff_offset) {
				DBG_PRINT("parse_app_exif() thumbnail offset is incorrect(0x%x < 0x%x)\r\n",  target_tiff_offset, current_tiff_offset);
				return JPEG_PARSE_FAIL;
			}
			return JPEG_PARSE_OK;
		}
		if (date_time_offset >= target_tiff_offset) {
			// Ignore date and time field if it is behind thumbnail image
			date_time_offset = 0;
		}

	case STATE_APP_EXIF_JUMP_DATE_TIME2:
		if (date_time_offset) {
			if (date_time_offset > current_tiff_offset) {
				if (date_time_offset-current_tiff_offset > (parse_end - parse_buf)) {
					if (application_len < parse_end - parse_buf) {
						DBG_PRINT("parse_app_exif() run out of application data\r\n");
						return JPEG_PARSE_FAIL;
					}
					application_len -= parse_end - parse_buf;
					current_tiff_offset += parse_end - parse_buf;
					parse_buf = parse_end;
					return JPEG_PARSE_NOT_DONE;
				}
				if (application_len < date_time_offset-current_tiff_offset) {
					DBG_PRINT("parse_app_exif() run out of application data\r\n");
					return JPEG_PARSE_FAIL;
				}
				application_len -= date_time_offset - current_tiff_offset;
				parse_buf += date_time_offset - current_tiff_offset;
				current_tiff_offset = date_time_offset;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			// Get Date and time here
			while (parsed_count < 20) {
				if (parsed_count==0 && (*parse_buf<'0' || *parse_buf>'9')) {
					break;
				}
				date_time[parsed_count++] = *parse_buf++;
				current_tiff_offset++;
				application_len--;

				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			date_time_offset = 0;
		}
		app_exif_state = STATE_APP_EXIF_JUMP_THUMBNAIL;

	case STATE_APP_EXIF_JUMP_THUMBNAIL:
		if (target_tiff_offset-current_tiff_offset > (parse_end - parse_buf)) {
			if (application_len < parse_end - parse_buf) {
				DBG_PRINT("parse_app_exif() run out of application data\r\n");
				return JPEG_PARSE_FAIL;
			}
			application_len -= parse_end - parse_buf;
			current_tiff_offset += parse_end - parse_buf;
			parse_buf = parse_end;
			return JPEG_PARSE_NOT_DONE;
		}
		app_exif_state = STATE_APP_EXIF_THUMBNAIL;
		if (application_len < target_tiff_offset-current_tiff_offset) {
			DBG_PRINT("parse_app_exif() run out of application data\r\n");
			return JPEG_PARSE_FAIL;
		}
		application_len -= target_tiff_offset - current_tiff_offset;
		parsed_count = 0;
		parse_buf += target_tiff_offset-current_tiff_offset;
		current_tiff_offset = target_tiff_offset;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}

	case STATE_APP_EXIF_THUMBNAIL:
		saved_main_state = main_state;				// Save current main state for returning from thumbnail parsing
		parsing_thumbnail = 1;
		jpeg_decode_parse_init();
		main_state = STATE_PARSING_THUMBNAIL;
		break;
	}

	return JPEG_PARSE_OK;
}

INT32S parse_frame(void)
{
	static INT16U frame_len;
	static INT8U count, comp_count;

	switch (frame_state) {
	case STATE_FRAME_LEN:
		if (!parsed_count) {
			frame_len = ((INT16U) *parse_buf++) << 8;

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		frame_len |= (INT16U) (*parse_buf++);
		frame_len -= 2;

		frame_state = STATE_PRECISION;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_PRECISION:
		if (*parse_buf!=0x8 && *parse_buf!=0xC) {
			DBG_PRINT("parse_frame(): Precision(%d) is not 0x08 nor 0x0C!\r\n", *parse_buf);
			return JPEG_PARSE_FAIL;
		}
		parse_buf++;

		frame_len--;
		frame_state = STATE_LINES;
		parsed_count = 0;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_LINES:
		if (!parsed_count) {
			if (!parsing_thumbnail) {
				image_height = ((INT16U) *parse_buf++) << 8;
			} else {
				thumbnail_image_height = ((INT16U) *parse_buf++) << 8;
			}

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		if (!parsing_thumbnail) {
			image_height |= (INT16U) (*parse_buf++);
			if (image_height > 8176) {
				image_height = 8176;
			}
		} else {
			thumbnail_image_height |= (INT16U) (*parse_buf++);
			if (thumbnail_image_height > 8176) {
				thumbnail_image_height = 8176;
			}
		}

		frame_len -= 2;
		frame_state = STATE_SAMPLES;
		parsed_count = 0;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_SAMPLES:
		if (!parsed_count) {
			if (!parsing_thumbnail) {
				image_width = ((INT16U) *parse_buf++) << 8;
			} else {
				thumbnail_image_width = ((INT16U) *parse_buf++) << 8;
			}

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		if (!parsing_thumbnail) {
			image_width |= (INT16U) (*parse_buf++);
		} else {
			thumbnail_image_width |= (INT16U) (*parse_buf++);
		}

		frame_len -= 2;
		frame_state = STATE_FRAME_COMP_NUM;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_FRAME_COMP_NUM:
		if (*parse_buf > MAX_FRAME_COMPONENT_NUM) {
			DBG_PRINT("parse_frame(): Component number(%d) is larger than %d!\r\n", *parse_buf, MAX_FRAME_COMPONENT_NUM);
			return JPEG_PARSE_FAIL;
		}
		comp_count = *parse_buf++;
		if (!parsing_thumbnail) {
			frame_comp_number = comp_count;
		} else {
			thumbnail_frame_comp_number = comp_count;
		}

		frame_len--;
		frame_state = STATE_FRAME_COMPONENT;
		count = 0;
		parsed_count = 0;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_FRAME_COMPONENT:
		while (count < comp_count) {
			if (!parsed_count) {
				if (!parsing_thumbnail) {
					frame_comp_identifier[count] = *parse_buf;	// Component identifier is allowed to be any value.
				} else {
					thumbnail_frame_comp_identifier[count] = *parse_buf;	// Component identifier is allowed to be any value.
				}
				parse_buf++;

				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (parsed_count == 1) {		// Component factor
				if (!parsing_thumbnail) {
					frame_comp_factor[count] = *parse_buf;
				} else {
					thumbnail_frame_comp_factor[count] = *parse_buf;
				}
				parse_buf++;

				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}
			if (!parsing_thumbnail || decode_thumbnail) {
				// Component quantization table identifier
				frame_comp_qtbl_index[count] = *parse_buf;
				if (frame_comp_qtbl_index[count] >= MAX_QUANTIZATION_NUM) {
					DBG_PRINT("parse_frame(): Quantization ID(%d) is larger than %d!\r\n", frame_comp_qtbl_index[count], MAX_QUANTIZATION_NUM-1);
					return JPEG_PARSE_FAIL;
				}
			}
			parse_buf++;

			parsed_count = 0;
			frame_len -= 3;
			count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		if (frame_len) {
			DBG_PRINT("parse_frame(): Unknown data(len=%d) in frame header!\r\n", frame_len);
			return JPEG_PARSE_FAIL;
		}
	}

	return JPEG_PARSE_OK;
}

INT32S parse_scan(void)
{
	static INT16U scan_len;
	static INT8U scan_comp_number;
	static INT8U frame_comp_idx, scan_comp_idx;
	INT16U mode;
	INT8U temp_char;
	INT32S status;

	switch (scan_state) {
	case STATE_SCAN_LEN:
		if (!parsed_count) {
			scan_len = ((INT16U) *parse_buf++) << 8;

			parsed_count++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		scan_len |= (INT16U) (*parse_buf++);
		scan_len -= 2;

		scan_state = STATE_SCAN_COMP_NUM;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_SCAN_COMP_NUM:
		scan_comp_number = *parse_buf++;
		if (scan_comp_number > MAX_SCAN_COMPONENT_NUM) {
			DBG_PRINT("parse_scan(): Component number(%d) is larger than %d!\r\n", scan_comp_number, MAX_SCAN_COMPONENT_NUM);
			return JPEG_PARSE_FAIL;
		}
		if (!parsing_thumbnail) {
			if (scan_comp_number > frame_comp_number) {
				DBG_PRINT("parse_scan(): Component number in scan header(%d) is larger than frame header(%d)!\r\n", scan_comp_number, frame_comp_number);
				return JPEG_PARSE_FAIL;
			}
		} else {
			if (scan_comp_number > thumbnail_frame_comp_number) {
				DBG_PRINT("parse_scan(): Component number in scan header(%d) is larger than frame header(%d)!\r\n", scan_comp_number, thumbnail_frame_comp_number);
				return JPEG_PARSE_FAIL;
			}
		}

		scan_len--;
		scan_state = STATE_SCAN_COMPONENT;
		frame_comp_idx = scan_comp_idx = 0;
		parsed_count = 0;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_SCAN_COMPONENT:
		while (scan_comp_idx < scan_comp_number) {
			if (!parsed_count) {
				temp_char = *parse_buf++;	// Scan component selector
				if (!parsing_thumbnail) {
					while (frame_comp_idx < frame_comp_number) {
						if (temp_char == frame_comp_identifier[frame_comp_idx]) {
							break;
						}
						frame_comp_idx++;
					}
					if (frame_comp_idx >= frame_comp_number) {
						DBG_PRINT("parse_scan(): Not matched identifier(0x%02X) is found in frame header!\r\n", temp_char);
						return JPEG_PARSE_FAIL;
					}
					scan_comp_factor[scan_comp_idx] = frame_comp_factor[frame_comp_idx];
				} else {
					while (frame_comp_idx < thumbnail_frame_comp_number) {
						if (temp_char == thumbnail_frame_comp_identifier[frame_comp_idx]) {
							break;
						}
						frame_comp_idx++;
					}
					if (frame_comp_idx >= thumbnail_frame_comp_number) {
						DBG_PRINT("parse_scan(): Not matched identifier(0x%02X) is found in frame header!\r\n", temp_char);
						return JPEG_PARSE_FAIL;
					}
					scan_comp_factor[scan_comp_idx] = thumbnail_frame_comp_factor[frame_comp_idx];
				}
				scan_comp_qtbl_index[scan_comp_idx] = frame_comp_qtbl_index[frame_comp_idx];

				parsed_count++;
				if (parse_buf >= parse_end) {
					return JPEG_PARSE_NOT_DONE;
				}
			}

			if (*parse_buf & 0xCC) {
				DBG_PRINT("parse_scan(): Entropy coding(huffman) selector(0x%02X) is not supported!\r\n", *parse_buf);
				return JPEG_PARSE_FAIL;
			}
			if (!parsing_thumbnail || decode_thumbnail) {
				scan_comp_huffman_index[scan_comp_idx] = *parse_buf;
			}
			parse_buf++;

			scan_len -= 2;
			parsed_count = 0;
			scan_comp_idx++;
			if (parse_buf >= parse_end) {
				return JPEG_PARSE_NOT_DONE;
			}
		}
		scan_state = STATE_START_SPECTRAL;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_START_SPECTRAL:
		if (*parse_buf++ > 0x3F) {
			DBG_PRINT("parse_scan(): Start of spectral(0x%02X) is larger than 0x3F!\r\n", *(parse_buf-1));
			return JPEG_PARSE_FAIL;
		}
		scan_len--;
		scan_state = STATE_END_SPECTRAL;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_END_SPECTRAL:
		if (*parse_buf++ > 0x3F) {
			DBG_PRINT("parse_scan(): End of spectral(0x%02X) is larger than 0x3F!\r\n", *(parse_buf-1));
			return JPEG_PARSE_FAIL;
		}
		scan_len--;
		scan_state = STATE_APPROXIMATION;
		if (parse_buf >= parse_end) {
			return JPEG_PARSE_NOT_DONE;
		}
	case STATE_APPROXIMATION:
		if (*parse_buf++ > 0xDD) {
			DBG_PRINT("parse_scan(): Approximation(0x%02X) is larger than 0xDD!\r\n", *(parse_buf-1));
			return JPEG_PARSE_FAIL;
		}
		if (--scan_len) {
			DBG_PRINT("parse_scan(): Unknown data(len=%d) in scan header!\r\n", scan_len);
			return JPEG_PARSE_FAIL;
		}
	}

	if (scan_comp_number == 1) {
		mode = C_JPG_CTRL_GRAYSCALE;
	} else if ((scan_comp_factor[0]==0x21) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV422;
	} else if ((scan_comp_factor[0]==0x22) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV420;
	} else if ((scan_comp_factor[0]==0x12) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV422V;
	} else if ((scan_comp_factor[0]==0x11) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV444;
	} else if ((scan_comp_factor[0]==0x41) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV411;
	} else if ((scan_comp_factor[0]==0x14) && (scan_comp_factor[1]==0x11) && (scan_comp_factor[2]==0x11)) {
		mode = C_JPG_CTRL_YUV411V;
	} else if ((scan_comp_factor[0]==0x22) && (scan_comp_factor[1]==0x21) && (scan_comp_factor[2]==0x21)) {
		mode = C_JPG_CTRL_YUV420H2;
	} else if ((scan_comp_factor[0]==0x22) && (scan_comp_factor[1]==0x12) && (scan_comp_factor[2]==0x12)) {
		mode = C_JPG_CTRL_YUV420V2;
	} else if ((scan_comp_factor[0]==0x41) && (scan_comp_factor[1]==0x21) && (scan_comp_factor[2]==0x21)) {
		mode = C_JPG_CTRL_YUV411H2;
	} else if ((scan_comp_factor[0]==0x14) && (scan_comp_factor[1]==0x12) && (scan_comp_factor[2]==0x12)) {
		mode = C_JPG_CTRL_YUV411V2;
	} else {
		DBG_PRINT("Unknown YUV mode\r\n");
		return JPEG_PARSE_FAIL;
	}
	if (!parsing_thumbnail) {
		yuv_mode = mode;
	} else {
		thumbnail_yuv_mode = mode;
	}

	if (parsing_thumbnail && !decode_thumbnail) {
		return JPEG_PARSE_OK;
	}

	if (!parsing_thumbnail) {
		if (jpeg_image_size_set(image_width, image_height)) {
			DBG_PRINT("Calling to jpeg_image_size_set(%d, %d) failed\r\n", image_width, image_height);
			return JPEG_SET_HW_ERROR;
		}
	} else {
		if (jpeg_image_size_set(thumbnail_image_width, thumbnail_image_height)) {
			DBG_PRINT("Calling to jpeg_image_size_set(%d, %d) failed\r\n", thumbnail_image_width, thumbnail_image_height);
			return JPEG_SET_HW_ERROR;
		}
	}

	if (jpeg_yuv_sampling_mode_set((INT32U) mode)) {
		DBG_PRINT("Calling to jpeg_yuv_sampling_mode_set(0x%x) failed\r\n", mode);
		return JPEG_SET_HW_ERROR;
	}

	if (scan_comp_number >= 1) {			// Luminance Y
		// Set Quantization table
		if (quantization_installed[scan_comp_qtbl_index[0]]) {
			if (jpeg_quant_luminance_set(0, 64, &quantization_table[scan_comp_qtbl_index[0]][0])) {
				DBG_PRINT("Calling to jpeg_quant_luminance_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}

		// Set Huffman Luminance DC table
		if (huffman_installed[(scan_comp_huffman_index[0]>>4)&0x1]) {		// 0 and 1 are DC, 2 and 3 are AC
			status = huffman_gen_table(1, 1, (scan_comp_huffman_index[0]>>4)&0x1);
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (jpeg_huffman_dc_lum_huffval_set(0, 16, &huffman_dc_values[(scan_comp_huffman_index[0]>>4)&0x1][0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_lum_huffval_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
		// Set Huffman Luminance AC table
		if (huffman_installed[0x2 | (scan_comp_huffman_index[0]&0x1)]) {		// 0 and 1 are DC, 2 and 3 are AC
			status = huffman_gen_table(1, 0, 0x2 | (scan_comp_huffman_index[0]&0x1));
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (jpeg_huffman_ac_lum_huffval_set(0, 256, &huffman_ac_values[scan_comp_huffman_index[0]&0x1][0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_lum_huffval_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
	}

	// Chrominance Cb/Cr
	if (scan_comp_number >= 2) {
		// Set Quantization table
		if (quantization_installed[scan_comp_qtbl_index[1]]) {
			if (jpeg_quant_chrominance_set(0, 64, &quantization_table[scan_comp_qtbl_index[1]][0])) {
				DBG_PRINT("Calling to jpeg_quant_chrominance_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		} else if (quantization_installed[scan_comp_qtbl_index[2]]) {
			if (jpeg_quant_chrominance_set(0, 64, &quantization_table[scan_comp_qtbl_index[2]][0])) {
				DBG_PRINT("Calling to jpeg_quant_chrominance_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}

		// Set Huffman Chrominance DC table
		if (huffman_installed[(scan_comp_huffman_index[1]>>4)&0x1]) {		// 0 and 1 are DC, 2 and 3 are AC
			status = huffman_gen_table(0, 1, (scan_comp_huffman_index[1]>>4)&0x1);
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (jpeg_huffman_dc_chrom_huffval_set(0, 16, &huffman_dc_values[(scan_comp_huffman_index[1]>>4)&0x1][0])) {
				DBG_PRINT("Calling to jpeg_huffman_dc_chrom_huffval_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
		// Set Huffman Chrominance AC table
		if (huffman_installed[0x2 | (scan_comp_huffman_index[1]&0x1)]) {		// 0 and 1 are DC, 2 and 3 are AC
			status = huffman_gen_table(0, 0, 0x2 | (scan_comp_huffman_index[1]&0x1));
			if (status != JPEG_PARSE_OK) {
				return status;
			}
			if (jpeg_huffman_ac_chrom_huffval_set(0, 256, &huffman_ac_values[scan_comp_huffman_index[1]&0x1][0])) {
				DBG_PRINT("Calling to jpeg_huffman_ac_chrom_huffval_set() failed\r\n");
				return JPEG_SET_HW_ERROR;
			}
		}
	}

	return JPEG_PARSE_OK;
}

void calcuate_image_width_height(void)
{
	if (!parsing_thumbnail) {
		if (yuv_mode == C_JPG_CTRL_YUV422) {
			extend_width = (image_width+0xF) & ~0xF;
			extend_height = (image_height+0x7) & ~0x7;
		} else if (yuv_mode == C_JPG_CTRL_YUV420) {
			extend_width = (image_width+0xF) & ~0xF;
			extend_height = (image_height+0xF) & ~0xF;
		} else if (yuv_mode == C_JPG_CTRL_YUV422V) {
			extend_width = (image_width+0x7) & ~0x7;
			extend_height = (image_height+0xF) & ~0xF;
		} else if (yuv_mode == C_JPG_CTRL_YUV444) {
			extend_width = (image_width+0x7) & ~0x7;
			extend_height = (image_height+0x7) & ~0x7;
		} else if (yuv_mode == C_JPG_CTRL_GRAYSCALE) {
			extend_width = (image_width+0x7) & ~0x7;
			extend_height = (image_height+0x7) & ~0x7;
		} else if (yuv_mode == C_JPG_CTRL_YUV411) {
			extend_width = (image_width+0x1F) & ~0x1F;
			extend_height = (image_height+0x7) & ~0x7;
		} else if (yuv_mode == C_JPG_CTRL_YUV411V) {
			extend_width = (image_width+0x7) & ~0x7;
			extend_height = (image_height+0x1F) & ~0x1F;
		} else if (yuv_mode == C_JPG_CTRL_YUV420H2) {
			extend_width = (image_width+0xF) & ~0xF;
			extend_height = (image_height+0xF) & ~0xF;
		} else if (yuv_mode == C_JPG_CTRL_YUV420V2) {
			extend_width = (image_width+0xF) & ~0xF;
			extend_height = (image_height+0xF) & ~0xF;
		} else if (yuv_mode == C_JPG_CTRL_YUV411H2) {
			extend_width = (image_width+0x1F) & ~0x1F;
			extend_height = (image_height+0x7) & ~0x7;
		} else if (yuv_mode == C_JPG_CTRL_YUV411V2) {
			extend_width = (image_width+0x7) & ~0x7;
			extend_height = (image_height+0x1F) & ~0x1F;
		} else {
			extend_width = image_width;
			extend_height = image_height;
		}
	} else {
		if (thumbnail_yuv_mode == C_JPG_CTRL_YUV422) {
			thumbnail_extend_width = (thumbnail_image_width+0xF) & ~0xF;
			thumbnail_extend_height = (thumbnail_image_height+0x7) & ~0x7;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV420) {
			thumbnail_extend_width = (thumbnail_image_width+0xF) & ~0xF;
			thumbnail_extend_height = (thumbnail_image_height+0xF) & ~0xF;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV422V) {
			thumbnail_extend_width = (thumbnail_image_width+0x7) & ~0x7;
			thumbnail_extend_height = (thumbnail_image_height+0xF) & ~0xF;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV444) {
			thumbnail_extend_width = (thumbnail_image_width+0x7) & ~0x7;
			thumbnail_extend_height = (thumbnail_image_height+0x7) & ~0x7;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_GRAYSCALE) {
			thumbnail_extend_width = (thumbnail_image_width+0x7) & ~0x7;
			thumbnail_extend_height = (thumbnail_image_height+0x7) & ~0x7;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV411) {
			thumbnail_extend_width = (thumbnail_image_width+0x1F) & ~0x1F;
			thumbnail_extend_height = (thumbnail_image_height+0x7) & ~0x7;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV411V) {
			thumbnail_extend_width = (thumbnail_image_width+0x7) & ~0x7;
			thumbnail_extend_height = (thumbnail_image_height+0x1F) & ~0x1F;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV420H2) {
			thumbnail_extend_width = (thumbnail_image_width+0xF) & ~0xF;
			thumbnail_extend_height = (thumbnail_image_height+0xF) & ~0xF;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV420V2) {
			thumbnail_extend_width = (thumbnail_image_width+0xF) & ~0xF;
			thumbnail_extend_height = (thumbnail_image_height+0xF) & ~0xF;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV411H2) {
			thumbnail_extend_width = (thumbnail_image_width+0x1F) & ~0x1F;
			thumbnail_extend_height = (thumbnail_image_height+0x7) & ~0x7;
		} else if (thumbnail_yuv_mode == C_JPG_CTRL_YUV411V2) {
			thumbnail_extend_width = (thumbnail_image_width+0x7) & ~0x7;
			thumbnail_extend_height = (thumbnail_image_height+0x1F) & ~0x1F;
		} else {
			thumbnail_extend_width = thumbnail_image_width;
			thumbnail_extend_height = thumbnail_image_height;
		}
	}
}

INT16U jpeg_decode_image_width_get(void)
{
	return image_width;
}

INT16U jpeg_decode_image_height_get(void)
{
	return image_height;
}

INT16U jpeg_decode_image_extended_width_get(void)
{
	return extend_width;
}

INT16U jpeg_decode_image_extended_height_get(void)
{
	return extend_height;
}

INT16U jpeg_decode_image_yuv_mode_get(void)
{
	return yuv_mode;
}

INT8U jpeg_decode_image_progressive_mode_get(void)
{
	return progressive_jpeg;
}

INT32S jpeg_decode_date_time_get(INT8U *target)
{
	INT8U i;

	if (date_time[0]) {
		for (i=0; i<20; i++) {
			target[i] = date_time[i];
		}
		return 0;
	}
	target[0] = 0x0;

	return -1;
}

INT8U jpeg_decode_thumbnail_exist_get(void)
{
	return thumbnail_exist;
}

INT16U jpeg_decode_thumbnail_width_get(void)
{
	return thumbnail_image_width;
}

INT16U jpeg_decode_thumbnail_height_get(void)
{
	return thumbnail_image_height;
}

INT16U jpeg_decode_thumbnail_extended_width_get(void)
{
	return thumbnail_extend_width;
}

INT16U jpeg_decode_thumbnail_extended_height_get(void)
{
	return thumbnail_extend_height;
}

INT16U jpeg_decode_thumbnail_yuv_mode_get(void)
{
	return thumbnail_yuv_mode;
}

INT8U * jpeg_decode_image_vlc_addr_get(void)
{
	return vlc_start;
}

INT32S jpeg_decode_output_set(INT32U y_addr, INT32U u_addr, INT32U v_addr, INT32U line)
{
	if (jpeg_yuv_addr_set(y_addr, u_addr, v_addr)) {
		return -1;
	}
	if (jpeg_fifo_line_set(line)) {
		return -1;
	}
	return 0;
}

void jpeg_decode_thumbnail_image_enable(void)
{
	decode_thumbnail = 1;
}

void jpeg_decode_thumbnail_image_disable(void)
{
	decode_thumbnail = 0;
}

INT32S jpeg_decode_clipping_range_set(INT16U start_x, INT16U start_y, INT16U width, INT16U height)
{
	return jpeg_image_clipping_range_set(start_x, start_y, width, height);
}

INT32S jpeg_decode_clipping_mode_enable(void)
{
	return jpeg_clipping_mode_enable();
}

INT32S jpeg_decode_clipping_mode_disable(void)
{
	return jpeg_clipping_mode_disable();
}

INT32S jpeg_decode_level2_scaledown_enable(void)
{
	return jpeg_level2_scaledown_mode_enable();
}

INT32S jpeg_decode_level2_scaledown_disable(void)
{
	return jpeg_level2_scaledown_mode_disable();
}

extern INT32S jpeg_level4_scaledown_mode_enable(void);				// Decompression: When level4 scale-down mode is enabled, output size will be 1/4 in width and height
extern INT32S jpeg_level4_scaledown_mode_disable(void);
INT32S jpeg_decode_level4_scaledown_enable(void)
{
	return jpeg_level4_scaledown_mode_enable();
}

INT32S jpeg_decode_level4_scaledown_disable(void)
{
	return jpeg_level4_scaledown_mode_disable();
}

INT32S jpeg_decode_level8_scaledown_enable(void)
{
	return jpeg_level8_scaledown_mode_enable();
}

INT32S jpeg_decode_level8_scaledown_disable(void)
{
	return jpeg_level8_scaledown_mode_disable();
}

INT32S jpeg_decode_vlc_maximum_length_set(INT32U len)
{
	return jpeg_vlc_maximum_length_set(len);
}

// Decodes complete JPEG VLC(variable length coding)
INT32S jpeg_decode_once_start(INT8U *buf, INT32U len)
{
	if (jpeg_decode_started) {
		DBG_PRINT("jpeg_decode_once_start(): JPEG decoding has already started\r\n");
		return -1;
	}

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
	jpeg_decode_started = 1;

	return 0;
}

// Decodes a sequence of JPEG VLC(variable length coding) buffers
INT32S jpeg_decode_on_the_fly_start(INT8U *buf, INT32U len)
{
	if (jpeg_decode_started) {
		if ((INT32U) buf & 0xF) {
			DBG_PRINT("Parameter buf is not 16-byte alignment in jpeg_decode_on_the_fly_start()\r\n");
			return -1;
		} else if (jpeg_multi_vlc_input_restart((INT32U) buf, len)) {
			DBG_PRINT("Calling to jpeg_multi_vlc_input_restart() failed\r\n");
			return -1;
		}

		return 0;
	} else {
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
		jpeg_decode_started = 1;
	}

	return 0;
}

INT32S jpeg_decode_output_restart(void)
{
	return jpeg_yuv_output_restart();
}

void jpeg_decode_stop(void)
{
	jpeg_stop();

	jpeg_decode_started = 0;
}

// Return the current status of JPEG engine
INT32S jpeg_decode_status_query(INT32U wait)
{
	return jpeg_status_polling(wait);
}


#endif
