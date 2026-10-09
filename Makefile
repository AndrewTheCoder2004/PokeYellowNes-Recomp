# Original Xbox Makefile (nxdk)
XBE_TITLE = ThunderEmperor
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(CURDIR)/main.c $(CURDIR)/../../core/src/thunder_core.c
CFLAGS += -I$(CURDIR)/../../core/include -O2

include $(NXDK_DIR)/Makefile
