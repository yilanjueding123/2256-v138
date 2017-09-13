#include "ap_state_resource.h"

t_FONT_TABLE_STRUCT  *number_font_cache;
static t_GP_RESOURCE_HEADER *resource_header;
static INT16U resource_handle;

//	prototypes
INT32S resource_read(INT32U offset_byte, INT8U *pbuf, INT32U byte_count);
INT32U ap_state_resource_string_load(INT16U language, INT16U index);
INT32S ap_state_resource_number_font_cache_init(void);
INT32S ap_state_resource_char_resolution_get(INT16U target_char, STRING_INFO *str_info, t_STRING_TABLE_STRUCT *str_res);
INT32S ap_state_resource_char_draw(INT16U target_char, INT16U *frame_buff, STRING_INFO *str_info);
INT16S ap_state_resource_time_stamp_position_x_get(void);
void ap_state_resource_time_stamp_position_y_set(INT16S y);

void ap_state_resource_time_stamp_position_x_set(INT16S x);
INT16S time_stamp_position_x, time_stamp_position_y;

INT32S ap_state_resource_init(void)
{
	INT32S i, font_cache_sts;
	
	if (resource_header) {
		ap_state_resource_exit();
	}
	resource_header = (t_GP_RESOURCE_HEADER *) gp_malloc(sizeof(t_GP_RESOURCE_HEADER) + (12*sizeof(t_FONT_TABLE_STRUCT)));
	if (!resource_header) {
		DBG_PRINT("Failed to allocate resource_header in umi_resource_init\r\n");
		return STATUS_FAIL;
	}
	number_font_cache = (t_FONT_TABLE_STRUCT *) (resource_header + 1);
	resource_handle = nv_open((INT8U *) "GPRS.PAK");
	if (resource_handle == 0xFFFF) {
		DBG_PRINT("Failed to open resource from NVRAM \r\n");
		gp_free((void *) resource_header);
		resource_header = NULL;
		return STATUS_FAIL;
	}
	if (resource_read( 0, (INT8U *) resource_header, sizeof(t_GP_RESOURCE_HEADER))) {
		DBG_PRINT("Failed to read resource_header in umi_resource_init\r\n");
		gp_free((void *) resource_header);
		resource_header = NULL;
		return STATUS_FAIL;
	}
	font_cache_sts = ap_state_resource_number_font_cache_init();
	if (font_cache_sts >= 0) {
		if (font_cache_sts) {
			for (i=0 ; i<font_cache_sts ; i++) {
				gp_free((void *) (number_font_cache + i)->font_content);
			}
		}
		number_font_cache = 0;
	}

	
	ap_state_resource_time_stamp_position_x_set(65);
	ap_state_resource_time_stamp_position_y_set(434);
	return STATUS_OK;
}

void ap_state_resource_exit(void)
{
	INT32U i;
	
	if (resource_header) {
		if (number_font_cache) {
			for (i=0 ; i<12 ; i++) {
				gp_free((void *) (number_font_cache + i)->font_content);
			}
		}
		gp_free((void *) resource_header);
		resource_header = NULL;
	}
}

void ap_state_resource_time_stamp_position_x_set(INT16S x)
{
	time_stamp_position_x = x; 
}

INT16S ap_state_resource_time_stamp_position_x_get(void)
{
	return time_stamp_position_x;
}
void ap_state_resource_time_stamp_position_y_set(INT16S y)
{
	time_stamp_position_y = y; 
}

INT16S ap_state_resource_time_stamp_position_y_get(void)
{
	return time_stamp_position_y;
}

INT32U ap_state_resource_string_load(INT16U language, INT16U index)
{
	INT32U offset, buff_addr;
	INT32S size;
	t_STRING_STRUCT string_header;

	offset = (resource_header->offset_string[language]) + (sizeof(t_STRING_STRUCT) * index);
	if (resource_read(offset, (INT8U *) &string_header, sizeof(t_STRING_STRUCT))) { //read string header
		DBG_PRINT("Failed to read string header in resource_string_load()\r\n");
		return NULL;
	}
	size = (string_header.length) * (resource_header->str_ch_width[language]);
	if (size <= 0) {
		return NULL;
	}
	buff_addr = (INT32U) gp_malloc(size);
	if (!buff_addr) {
		return NULL;
	}
	if (resource_read(string_header.raw_data_offset, (INT8U *) buff_addr, size)) { //read string
		gp_free((void *) buff_addr);
		DBG_PRINT("Failed to read string data in resource_string_load()\r\n");
		return NULL;
	}
	return buff_addr;
}

INT32S ap_state_resource_char_resolution_get(INT16U target_char, STRING_INFO *str_info, t_STRING_TABLE_STRUCT *str_res)
{
	INT32U offset;
	t_FONT_STRUCT font_header;
	t_FONT_TABLE_STRUCT font;
	
	if(str_info->language == LCD_EN) {
		target_char -= 0x0020;
	}
	if (str_info->language == LCD_EN && (target_char > 0xE && target_char < 0x1B) && number_font_cache) {
		str_res->string_width += (number_font_cache + (target_char - 0xF))->font_width;
		str_res->string_height += (number_font_cache + (target_char - 0xF))->font_height;
	} else {
		offset = (resource_header->offset_font[str_info->language]) + (sizeof(t_FONT_STRUCT) * str_info->font_type);
		if (resource_read(offset, (INT8U *) &font_header, sizeof(t_FONT_STRUCT))) { //read font header
			DBG_PRINT("Failed to read font header in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
		//get target charactor table offset
		offset = font_header.raw_data_offset + (target_char * sizeof(t_FONT_TABLE_STRUCT));
		if (resource_read(offset, (INT8U *) &font, sizeof(t_FONT_TABLE_STRUCT))) { //read font table
			DBG_PRINT("Failed to read font table in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
		str_res->string_width += font.font_width;
		if (str_res->string_height < font.font_height) {
			str_res->string_height = font.font_height;
		}
	}
	return STATUS_OK;
}

INT32S ap_state_resource_string_resolution_get(STRING_INFO *str_info, t_STRING_TABLE_STRUCT *str_res)
{
	INT32U str_addr, iStrLoc;
	INT16U character = 0;
	
	str_addr = ap_state_resource_string_load(str_info->language, str_info->str_idx);
	if (!str_addr) {
		DBG_PRINT("Failed load string\r\n");
		return STATUS_FAIL;
	}
	str_res->string_width = 0;
	str_res->string_height = 0;
	iStrLoc = 0;
	while(1) {
		if (str_info->language == LCD_EN) {
   			character = (INT16U ) (((char *) str_addr)[iStrLoc]);
   			if (character == 0) {
   				break;
   			}
   			if((character<0x20)||character>0x7f) {
				character = '_';
			}
			iStrLoc++;
		} else {
   			character = ((INT16U *) str_addr)[iStrLoc];
   			if (character == 0) {
   				break;
   			}
			iStrLoc++;
		}
		if (ap_state_resource_char_resolution_get(character, str_info, str_res)) {
			gp_free((void *) str_addr);
			DBG_PRINT("Failed to draw char font\r\n");
			return STATUS_FAIL;
		}
	}
	gp_free((void *) str_addr);
	return STATUS_OK;
}

INT32S ap_state_resource_number_font_cache_init(void)
{
	INT32U offset, len, i;
	INT8U *input_buffer;
	t_FONT_STRUCT font_header;
	t_FONT_TABLE_STRUCT *font;
	
	for (i=0 ; i<12 ; i++) {
		font = number_font_cache + i;
		offset = resource_header->offset_font[LCD_EN];
		if (resource_read(offset, (INT8U *) &font_header, sizeof(t_FONT_STRUCT))) { //read font header
			DBG_PRINT("Failed to read font header in ap_state_resource_number_font_cache_init()\r\n");
			return i;
		}
		//get target charactor table offset
		offset = font_header.raw_data_offset + ((i + 0xF) * sizeof(t_FONT_TABLE_STRUCT));
		if (resource_read(offset, (INT8U *) font, sizeof(t_FONT_TABLE_STRUCT))) { //read font table
			DBG_PRINT("Failed to read font table in ap_state_resource_number_font_cache_init()\r\n");
			return i;
		}
		len = font->font_height * font->bytes_per_line;
		input_buffer = (INT8U *) gp_malloc(len);
		if (!input_buffer) {
			DBG_PRINT("Failed to allocate input_buffer in ap_state_resource_number_font_cache_init()\r\n");
			return i;
		}
		if (resource_read(font_header.raw_data_offset + font->font_content, input_buffer, len)) { //read font content
		    gp_free(input_buffer);
		    DBG_PRINT("Failed to read font data in ap_state_resource_number_font_cache_init()\r\n");
			return (i+1);
		}
		font->font_content = (INT32U) input_buffer;
	}
	return -1;
}

INT32S ap_state_resource_char_draw(INT16U target_char, INT16U *frame_buff, STRING_INFO *str_info)
{
	INT32U offset, offset_pixel, font_data, offset_tmp, byte_tmp, len, i, j, width, height;
	INT8U *input_buffer;
	t_FONT_STRUCT font_header;
	t_FONT_TABLE_STRUCT font;
	
	if(str_info->language == LCD_EN) {
		target_char -= 0x0020;
	}
	if (str_info->language == LCD_EN && (target_char > 0xE && target_char < 0x1B) && number_font_cache) {
		offset = (INT32U) (number_font_cache + (target_char - 0xF));
		gp_memcpy((INT8S *) &font, (INT8S *) offset, sizeof(t_FONT_TABLE_STRUCT));
		input_buffer = (INT8U *) (((t_FONT_TABLE_STRUCT *) offset)->font_content);
		len = 0;
	} else {
		offset = (resource_header->offset_font[str_info->language]) + (sizeof(t_FONT_STRUCT) * str_info->font_type);
		if (resource_read(offset, (INT8U *) &font_header, sizeof(t_FONT_STRUCT))) { //read font header
			DBG_PRINT("Failed to read font header in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
		//get target charactor table offset
		offset = font_header.raw_data_offset + (target_char * sizeof(t_FONT_TABLE_STRUCT));
		if (resource_read(offset, (INT8U *) &font, sizeof(t_FONT_TABLE_STRUCT))) { //read font table
			DBG_PRINT("Failed to read font table in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
		len = font.font_height * font.bytes_per_line;
		input_buffer = (INT8U *) gp_malloc(len);
		if (!input_buffer) {
			DBG_PRINT("Failed to allocate input_buffer in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
		if (resource_read(font_header.raw_data_offset + font.font_content, input_buffer, len)) { //read font content
		    gp_free(input_buffer);
		    DBG_PRINT("Failed to read font data in resource_font_load()\r\n");
			return STATUS_FAIL;
		}
	}
	width = str_info->buff_w;
	height = str_info->buff_h;
	//draw
	font_data = 0;
	for (i=0 ; i<font.font_height ; i++) {
		offset_tmp = (str_info->pos_y + i)*width + str_info->pos_x;
		byte_tmp = i*font.bytes_per_line;
		for (j=0 ; j<font.font_width ; j++) {
			if (!(j&7)) {
				font_data = *(input_buffer + (j>>3) + byte_tmp);
			}
			if (font_data & (1 << (7 - (j&7)))) {
				offset_pixel = offset_tmp + j;
				if (offset_pixel <= width*height) {
					*(frame_buff + offset_pixel) = str_info->font_color;
				}
			}
		}
	}	
	str_info->pos_x += font.font_width;
	if (len) {
		gp_free((void *) input_buffer);
	}
	return STATUS_OK;
}

INT32S ap_state_resource_string_draw(INT16U *frame_buff, STRING_INFO *str_info)
{
	INT32U str_addr, iStrLoc;
	INT16U character = 0;
	
	str_addr = ap_state_resource_string_load(str_info->language, str_info->str_idx);
	if (!str_addr) {
		DBG_PRINT("Failed load string\r\n");
		return STATUS_FAIL;
	}
	iStrLoc = 0;
	while(1) {
		if (str_info->language == LCD_EN) {
   			character = (INT16U ) (((char *) str_addr)[iStrLoc]);
   			if (character == 0) {
   				break;
   			}
   			if((character<0x20)||character>0x7f) {
				character = '_';
			}
			iStrLoc++;
		} else {
   			character = ((INT16U *) str_addr)[iStrLoc];
   			if (character == 0) {
   				break;
   			}
			iStrLoc++;
		}
		if (ap_state_resource_char_draw(character, frame_buff, str_info)) {
			gp_free((void *) str_addr);
			DBG_PRINT("Failed to draw char font\r\n");
			return STATUS_FAIL;
		}
	}
	gp_free((void *) str_addr);
	return STATUS_OK;
}

INT32S ap_state_resource_string_ascii_draw(INT16U *frame_buff, STRING_ASCII_INFO *str_ascii_info)
{
	STRING_INFO str_info;
	INT32U iStrLoc;
	INT16U character = 0;
	
	gp_memcpy((INT8S *) &str_info, (INT8S *) str_ascii_info, 12);
	str_info.language = LCD_EN;
	iStrLoc = 0;
	while(1) {
		character = (INT16U ) ((str_ascii_info->str_ptr)[iStrLoc]);
		if (character == 0) {
			break;
		}
		if((character<0x20)||character>0x7f) {
			character = '_';
		}
		iStrLoc++;
		if (ap_state_resource_char_draw(character, frame_buff, &str_info)) {
			DBG_PRINT("Failed to draw char font\r\n");
			return STATUS_FAIL;
		}
	}
	return STATUS_OK;
}

INT16U ap_state_resource_language_num_get(void)
{
	if (!resource_header) {
		return 0;
	}
	return resource_header->language_num;
}

INT32S ap_state_resource_user_option_load(SYSTEM_USER_OPTION *user_option)
{
	if (resource_read(resource_header->offset_factor_default_option, (INT8U *) user_option, sizeof(SYSTEM_USER_OPTION))) {
		DBG_PRINT("Failed to read user config in resource_user_items_load()\r\n");
		return STATUS_FAIL;
	}
	return STATUS_OK;
}

INT32S resource_read(INT32U offset_byte, INT8U *pbuf, INT32U byte_count)
{
	nv_lseek(resource_handle, offset_byte, SEEK_SET);
	if (nv_read(resource_handle, (INT32U) pbuf, byte_count)) {
		return STATUS_FAIL;
	}
	return STATUS_OK;
}

INT32S ap_state_resource_char_draw_osd(INT16U target_char, INT16U *frame_buff, STRING_INFO *str_info, INT8U type, INT8U num_type)
{
	INT32U  offset_pixel, font_data, offset_tmp, byte_tmp, len, j, width, height,k;
	INT32S i, buf_h_idx;
	const INT8U *input_buffer;
	//t_FONT_STRUCT font_header;
	t_FONT_TABLE_STRUCT font;	
	
	
	font.font_height = 40;
	font.font_width = 19;
	font.bytes_per_line = 3;
	byte_tmp = font.bytes_per_line;
	
	if(target_char==0x2F)
	{
		input_buffer = acFontHZArial017Slash+str_info->font_offset_h*byte_tmp;
	}
	else if(target_char==0x3A)
	{
		input_buffer = acFontHZArial017Comma+str_info->font_offset_h*byte_tmp;
	}
	else
	{
		target_char -= 0x0030;
		input_buffer = number[target_char]+str_info->font_offset_h*byte_tmp;
	}
	
	width = str_info->buff_w;
	height = str_info->buff_h;

	//draw
//	font_data = 0;
	for (i=str_info->font_offset_h,buf_h_idx = 0; i<font.font_height; i++,buf_h_idx++) {

		if((str_info->pos_y + buf_h_idx) >= str_info->buff_h) {
			break;
		}

		if (type == YUV420_DRAW) {
			if ((str_info->pos_y + buf_h_idx) & 0x1) {
				offset_tmp = ((str_info->pos_y + buf_h_idx-1)*width*3>>1) + (width<<1) + str_info->pos_x;
			} else {
				offset_tmp = ((str_info->pos_y + buf_h_idx)*width*3>>1) + str_info->pos_x;
			}
		} else if(type == YUYV_DRAW) {
			offset_tmp = (str_info->pos_y + buf_h_idx)*width + (str_info->pos_x & ~0x0001);
		} else {
			offset_tmp = (str_info->pos_y + buf_h_idx)*width + str_info->pos_x;
		}
		
		for(k=0;k<font.bytes_per_line;k++)
		{
			//if(k<2)
			{
				len = 8;
			}
			//else
			//{
			///	len = 3;
			//}
			offset_tmp = offset_tmp+8;
			font_data = *input_buffer++;
			for (j=0 ; j<len ; j++) {
				//loop unrolling
				//if (font_data & (1 << (7 - (j&7)))) {
				if (font_data & 0x80) {
					offset_pixel = offset_tmp + j;
					if (type == YUV420_DRAW) {
						*((INT8U *)frame_buff + offset_pixel) = 0xFF;
					} else {
						*(frame_buff + offset_pixel) = str_info->font_color;
					}
				}
				else
				{
					offset_pixel = offset_tmp + j;
				}
				font_data=font_data<<1;
			}
		}
	}
	str_info->pos_x += font.font_width;
	return STATUS_OK;
}	

