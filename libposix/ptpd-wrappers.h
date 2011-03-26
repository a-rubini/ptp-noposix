/*
 * This header is used to hide all posix functions that can be hidden
 */
#ifndef __PTPD_WRAPPERS_H__
#define __PTPD_WRAPPERS_H__


#ifdef __STDC_HOSTED__
/*
 * The compiler is _not_ freestanding: we need to include some headers that
 * are not available in the freestanding compilation, so are missing from
 * source files.
 */
#include <string.h>

#endif


#endif /* __PTPD_WRAPPERS_H__ */
