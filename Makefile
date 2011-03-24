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
	-include compat.h
LDFLAGS = -L. -lwripc -lptpnetif

# Flags from the original Makefiles
CFLAGS += -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM
CFLAGS += -DDEBUG
#CFLAGS += -DPTPD_DBG


# Targets follows
all: check libs ptpd

# The objects are all from the ptp directory
D = PTPWRd
# These have no warnings
OBJS = $D/ptpd.o
OBJS += $D/bmc.o
OBJS += $D/dep/servo.o
OBJS += $D/dep/sys.o

# The following ones have header problems (warning: implicit declaration)
OBJS += $D/protocol.o
OBJS += $D/wr_protocol.o
OBJS += $D/dep/net.o
OBJS += $D/ptpd_exports.o

# Other warnings
OBJS += $D/arith.o
OBJS += $D/display.o
OBJS += $D/dep/msg.o
OBJS += $D/dep/startup.o
OBJS += $D/dep/timer.o
OBJS += $D/dep/wr_servo.o

# Temporarily, add this one, until I get rid of this assembly
OBJS += ./libwripc/helper_arm.o



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
ptpd: check libs $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o ptpd


# clean and so on.
clean:
	rm -f *.a */*.o */dep/*.o *~ */*~

distclean:
	git clean -f -d

# even if there are files with these names, ignore them
.PHONY: all check libs clean distclean
