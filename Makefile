
#.PATH: ${SRCTOP}/sys/dev/iwx

KMOD    = if_iwx
SRCS    = if_iwx.c device_if.h bus_if.h pci_if.h opt_iwx.h opt_wlan.h

.include <bsd.kmod.mk>
