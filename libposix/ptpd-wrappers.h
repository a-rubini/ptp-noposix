/*
 * This header is used to hide all posix functions that can be hidden
 */
#ifndef __PTPD_WRAPPERS_H__
#define __PTPD_WRAPPERS_H__

extern void *ptpd_wrap_malloc(int size);
extern void *ptpd_wrap_memset(void *s, int c, int n);
extern unsigned int ptpd_wrap_sleep(unsigned int seconds);
extern char *ptpd_wrap_strcpy(char *dest, const char *src);
extern char *ptpd_wrap_strncpy(char *dest, const char *src, int n);


static inline void *__calloc(int nmemb, int size)
{
	void *ret = ptpd_wrap_malloc(nmemb * size);
	if (ret)
		ptpd_wrap_memset(ret, 0, nmemb * size);
	return ret;
}


#endif /* __PTPD_WRAPPERS_H__ */
