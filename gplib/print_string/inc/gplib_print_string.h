#ifndef __GPLIB_PRINT_STRING_H__
#define __GPLIB_PRINT_STRING_H__


#include "gplib.h"

#if GPLIB_PRINT_STRING_EN == 1
extern void print_string(CHAR *fmt, ...);
extern INT8U* uart_recive_data(INT8U* buf);
#endif
extern void get_string(CHAR *s);


#endif 		// __GPLIB_PRINT_STRING_H__