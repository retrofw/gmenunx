PLATFORM = retrofw

CFLAGS += -Os
CFLAGS += -mhard-float -mips32 -mno-mips16

LDFLAGS = -lintl

include gmenunx.mk
