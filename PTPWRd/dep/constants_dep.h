
/* constants_dep.h */

#ifndef CONSTANTS_DEP_H
#define CONSTANTS_DEP_H

/**
*\file
* \brief Plateform-dependent constants definition
* 
* This header defines all includes and constants which are plateform-dependent
* 
* ptpdv2 is only implemented for linux, NetBSD and FreeBSD
 */

/* platform dependent */

#if !__STDC_HOSTED__  /* This is the freestanding version: no OS help here */

/* fake some numbers */
#define IF_NAMESIZE		8
#define INET_ADDRSTRLEN		16

#endif /* freestanding */


#ifdef	linux
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>

#define BSD_INTERFACE_FUNCTIONS

/* -- don't define endianness here, it's already done in the Makefile --
#include<endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define PTPD_LSBF
#elif __BYTE_ORDER == __BIG_ENDIAN
#define PTPD_MSBF
#endif
-- endianness */

#endif /* linux */


#if defined(__NetBSD__) || defined(__FreeBSD__)
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <net/if.h>
# include <net/if_dl.h>
# include <net/if_types.h>
# if defined(__FreeBSD__)
#  include <net/ethernet.h>
#  include <sys/uio.h>
# else
#  include <net/if_ether.h>
# endif
# include <ifaddrs.h>

//# define adjtimex ntp_adjtime
/* -- don't define endianness here, it's already done in the Makefile --
# include <machine/endian.h>
# if BYTE_ORDER == LITTLE_ENDIAN
#   define PTPD_LSBF
# elif BYTE_ORDER == BIG_ENDIAN
#   define PTPD_MSBF
# endif
-- endianness */

#endif /* bsd */

/* Common definitions follow */
#define IFACE_NAME_LENGTH         16 //IF_NAMESIZE
#define NET_ADDRESS_LENGTH        INET_ADDRSTRLEN
# define IFCONF_LENGTH 10


#define CLOCK_IDENTITY_LENGTH 8
#define ADJ_FREQ_MAX  512000

/* UDP/IPv4 dependent */

#define SUBDOMAIN_ADDRESS_LENGTH  4
#define PORT_ADDRESS_LENGTH       2
#define PTP_UUID_LENGTH			  6
#define CLOCK_IDENTITY_LENGTH	  8
#define FLAG_FIELD_LENGTH		  2

#define PACKET_SIZE  1024 //ptpdv1 value kept because of use of TLV...

#define PTP_EVENT_PORT    319
#define PTP_GENERAL_PORT  320

#define DEFAULT_PTP_DOMAIN_ADDRESS     "224.0.1.129"
#define PEER_PTP_DOMAIN_ADDRESS     "224.0.0.107"

#define MM_STARTING_BOUNDARY_HOPS  0x7fff

/* others */

#define SCREEN_BUFSZ  128
#define SCREEN_MAXSZ  80
#endif /*CONSTANTS_DEP_H_*/
