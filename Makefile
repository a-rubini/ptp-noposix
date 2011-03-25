# Standard stanza for cross-compilation (courtesy of the linux makefile)
AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

# Flags for standalone compilation
TOPDIR=$(shell /bin/pwd)
CFLAGS = -Wall -ggdb -I$(TOPDIR)/wrsw_hal -I$(TOPDIR)/libwripc \
	-I$(TOPDIR)/libptpnetif -I$(TOPDIR)/PTPWRd -I$(LINUX)/include \
	-include compat.h -include libposix/ptpd-wrappers.h
# These are lifted in the ptp.o temporary object file, for me to see the size
CORELIBS= libwripc.a libptpnetif.a

LDFLAGS = #-L. -lwripc -lptpnetif

# Flags from the original Makefiles
CFLAGS += -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM
CFLAGS += -DDEBUG
#CFLAGS += -DPTPD_DBG


# Targets follows
all: check libs ptpd

# The objects are all from the ptp directory
D = PTPWRd
OBJS = $D/ptpd.o
OBJS += $D/arith.o
OBJS += $D/bmc.o
OBJS += $D/dep/msg.o
OBJS += $D/dep/net.o
OBJS += $D/dep/servo.o
OBJS += $D/dep/startup.o
OBJS += $D/dep/sys.o
OBJS += $D/dep/timer.o
OBJS += $D/dep/wr_servo.o
OBJS += $D/display.o
OBJS += $D/protocol.o
OBJS += $D/ptpd_exports.o
OBJS += $D/wr_protocol.o

# Temporarily, add this one, until I get rid of this assembly
OBJS += ./libwripc/helper_arm.o

# This is the compatilibity library, to hide posix stuff in a single place
LATE_OBJS = ./libposix/posix-wrapper.o
#OBJS += ./libcompat/freestanding-wrapper.o


# we only support cross-compilation (if you want force CROSS_COMPILE to " ")
# similarly, we need a kernel at this time
check:
	@if [ -z "$$CROSS_COMPILE" ]; then \
		echo "Please set CROSS_COMPILE" >& 2; exit 1; \
	fi
	@if [ -z "$$LINUX" ]; then \
		echo "Please set LINUX for header inclusion" >& 2; exit 1; \
	fi

libs: check libwripc.a libptpnetif.a

libwripc.a: libwripc/wr_ipc.o
	$(AR) r $@ $^

libptpnetif.a: libptpnetif/hal_client.o libptpnetif/ptpd_netif.o
	$(AR) r $@ $^

# the binary is just a collection of object files
ptpd: check libs $(OBJS) $(LATE_OBJS)
	$(LD) -r $(OBJS) $(CORELIBS) -o ptpd.o
	$(CC) $(CFLAGS) ptpd.o $(LATE_OBJS) $(LDFLAGS) -o ptpd


# clean and so on.
clean:
	rm -f *.a */*.o */dep/*.o *~ */*~

distclean:
	git clean -f -d

# even if there are files with these names, ignore them
.PHONY: all check libs clean distclean
