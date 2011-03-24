# Makefile for ptpd


ifeq ($(TARGET),ARM)
		include ../../Makedefs
		CC=$(CROSS_COMPILE_ARM)gcc
else
		CC=gcc
endif

RM = rm -f
CFLAGS = -I. -O2 -I../include -DDEBUG -I../../kernel/include -I../wrsw_hal \
	 -I../libptpnetif -I../libwripc
 
CPPFLAGS = -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM #-DPTPD_DBG

#### use new single state machine (new wrspec) #####
#CPPFLAGS += -DNEW_SINGLE_WRFSM
###################################################

#### show message content ####
CPPFLAGS += #-DPTPD_DBGMSG


#### this one shows function names ####
CPPFLAGS += #-DPTPD_DBG_S_FUN

####detailed debugging: ####
#CPPFLAGS +=-DPTPD_DBGV

#####old flags: ######
#CPPFLAGS +=-DWR_NETIF
#CPPLFAGS +=-DCONFIG_WRPTP
#CPPFLAGS += -DWR_IN_HEADER_AND_ANNBODY

LDFLAGS = -lm 
ifeq ($(TARGET),ARM)
	LDFLAGS +=-L../libwripc -lwripc  # -L../libptpnetif -lptpnetif 
endif

PROG = wr_ptpd

OBJ  = ptpd.o arith.o bmc.o wr_protocol.o protocol.o display.o\
	dep/msg.o dep/net.o dep/servo.o dep/startup.o dep/sys.o dep/timer.o dep/wr_servo.o ptpd_exports.o

ifeq ($(TARGET),ARM)
	OBJ += ../libptpnetif/hal_client.o ../libptpnetif/ptpd_netif.o 
else
	OBJ += dep/ptpd_netif/ptpd_netif_mch.o
endif
	
HDR  = ptpd.h constants.h datatypes.h \
	dep/ptpd_dep.h dep/constants_dep.h dep/datatypes_dep.h \


.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(OBJ): $(HDR)

pc:	all

run:	all
	scp $(PROG) root@192.168.1.16:/tmp
	scp $(PROG) root@192.168.1.6:/tmp
	
clean:
	$(RM) $(PROG) $(OBJ)

deploy:
	cp $(PROG) $(WR_INSTALL_ROOT)/bin