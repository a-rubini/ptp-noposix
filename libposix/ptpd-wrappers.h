/*
 * This header is used to hide all posix functions that can be hidden
 */
#ifndef __PTPD_WRAPPERS_H__
#define __PTPD_WRAPPERS_H__

#if __STDC_HOSTED__
/*
 * The compiler is _not_ freestanding: we need to include some headers that
 * are not available in the freestanding compilation, so are missing from
 * source files.
 */
#include <stdint.h>

#else

#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
/*
 * This is a freestanding compilation, and we may miss some data
 * structures. For example misses <stdint.h>. Most likely it's
 * because it's an old compiler version, so the #if may be wrong here.
 */

/* Looks like we miss <stdint.h>. Let's assume we are 32 bits */
/*typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long	uint64_t;

typedef signed char		int8_t;
typedef signed short		int16_t;
typedef signed int		int32_t;
typedef signed long long	int64_t;
*/
/* Hmm... htons/htonl are missing. I made the Makefile check endianness */
#ifdef PTPD_MSBF
static inline uint16_t htons(uint16_t x) {return x;}
static inline uint32_t htonl(uint32_t x) {return x;}
static inline uint16_t ntohs(uint16_t x) {return x;}
#else
static inline uint16_t htons(uint16_t x) { return (x << 8) | (x >> 8); }
static inline uint32_t htonl(uint32_t x)
{ return htons(x>>16) | ((uint32_t)(htons(x) << 16));}
#endif /* endian */

extern int usleep(useconds_t usec);


/* The exports are not used in freestanding environment */
static inline void ptpd_init_exports() {}
static inline void ptpd_handle_wripc() {}

#define printf(x, ...) mprintf(x, ##__VA_ARGS__)
#define fprintf(file, x, ...) mprintf(x, ##__VA_ARGS__)
//#define sprintf(buf, ...) msprintf(buf, __VA_ARGS__)
//#define DBG(x, ...) mprintf(x, ##__VA_ARGS__)



#endif /* hosted */


#endif /* __PTPD_WRAPPERS_H__ */
