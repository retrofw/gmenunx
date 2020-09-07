PLATFORM = retrofw

CROSS_COMPILE ?= /opt/mipsel-RetroFW-linux-uclibc/usr/bin/mipsel-linux-

export CROSS_COMPILE

CFLAGS += -Os
CFLAGS += -mhard-float -mips32 -mno-mips16

LDFLAGS = -lintl

include gmenunx.mk
