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
CORELIBS = libwripc.a libptpnetif.a

LDFLAGS = #-L. -lwripc -lptpnetif

# Flags from the original Makefiles
CFLAGS += -DPTPD_NO_DAEMON -DNEW_SINGLE_WRFSM
CFLAGS += -DDEBUG
#CFLAGS += -DPTPD_DBG


# Targets follows (note that the freestanding version is only an object
all: check libs ptpd ptpd-freestanding.o

# The main objects are all from the ptp directory
D = PTPWRd
OBJS = $D/ptpd.o
OBJS += $D/arith.o
OBJS += $D/bmc.o
OBJS += $D/dep/msg.o
OBJS += $D/dep/net.o
OBJS += $D/dep/servo.o
OBJS += $D/dep/sys.o
OBJS += $D/dep/timer.o
OBJS += $D/dep/wr_servo.o
OBJS += $D/display.o
OBJS += $D/protocol.o
OBJS += $D/ptpd_exports.o
OBJS += $D/wr_protocol.o

# The following object is so posix-specific, that it mut go alone
POSIX_OBJS = $D/dep/startup.o

# This is a replacement for startup in the freestanding version
FREE_OBJS = libposix/freestanding-startup.o

# When running freestanding, we have no libraries, so hack this
FREE_OBJS += libposix/wr_nolibs.o

# This is the compatilibity library, to hide posix stuff in a single place
POSIX_OBJS += libposix/posix-wrapper.o
FREE_OBJS += libposix/freestanding-wrapper.o

# we only support cross-compilation (if you want force CROSS_COMPILE to " ")
# similarly, we need a kernel at this time
check:
	@if [ -z "$$CROSS_COMPILE" ]; then \
		echo "Please set CROSS_COMPILE" >& 2; exit 1; \
	fi
	@if [ -z "$$LINUX" ]; then \
		echo "Please set LINUX for header inclusion" >& 2; exit 1; \
	fi

libs: check $(CORELIBS)

libwripc.a: libwripc/wr_ipc.o libwripc/helper_arm.o
	$(AR) r $@ $^

libptpnetif.a: libptpnetif/hal_client.o libptpnetif/ptpd_netif.o
	$(AR) r $@ $^

# the binary is just a collection of object files.
# However, we need a freestanding version, so we build two binaries:
# one is gnu/linux-based (well, "posix") and the other is freestanding.
# The "ptpd.o" object is used to run "nm" on it and drive patches
ptpd: check $(CORELIBS) ptpd.o $(CORELIBS) $(POSIX_OBJS)
	$(CC) $(CFLAGS) ptpd.o $(CORELIBS) $(POSIX_OBJS) $(LDFLAGS) -o ptpd


# make an intermediavte version here as well, before linking in main
ptpd-freestanding.o: check ptpd.o $(FREE_OBJS)
	$(LD) -r ptpd.o $(FREE_OBJS) -o $@

# This is not built by default, only on explicit request
ptpd-freestanding: ptpd-freestanding.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $(@:.o=)

ptpd.o: $(OBJS)
	$(LD) -r $(OBJS) -o $@

# This is a target for me: print all headers included by this tree
printh:
	grep -h '^#include' $(OBJS:.o=.c) $$(find . -name \*.h) | \
		sort | uniq -c

printhf:
	grep '^#include' $(OBJS:.o=.c) $$(find . -name \*.h) | \
		sort -k 2


# clean and so on.
clean:
	rm -f *.a */*.o */dep/*.o *~ */*~

distclean:
	git clean -f -d

# even if there are files with these names, ignore them
.PHONY: all check libs clean distclean printh printhf
