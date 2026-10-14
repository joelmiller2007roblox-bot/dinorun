##
## Dino Run - PSL1GHT build
##
## Needs the PSL1GHT toolchain installed (PS3DEV and PSL1GHT env vars set).
##   make          -> dinorun.self
##   make pkg      -> dinorun.pkg  (install on a HEN / CFW PS3)
##

PREFIX      := ppu

TARGET      := dinorun
TITLE       := Dino Run
APPID       := DINORUN01
CONTENTID   := UP0001-$(APPID)_00-0000000000000000
ICON0       := $(PS3DEV)/bin/ICON0.PNG
SFOXML      := $(PS3DEV)/bin/sfo.xml

FLAGS       := -std=gnu99
INCLUDES    := -Iinclude
LIBS        := -lsimdmath -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

CC          := $(PREFIX)-gcc
LD          := $(PREFIX)-gcc

CFLAGS      := -Wall -O2 $(FLAGS) -I$(PSL1GHT)/$(PREFIX)/include $(INCLUDES)
LDFLAGS     := -L$(PSL1GHT)/$(PREFIX)/lib $(LIBS)

MAKE_SELF   := make_self_npdrm
MAKE_SFO    := sfo.py
MAKE_PKG    := pkg.py

SOURCES     := source/main.c source/rsxutil.c
OFILES      := $(SOURCES:.c=.o)

default: $(TARGET).self

clean:
	rm -f $(OFILES) $(TARGET).elf $(TARGET).self $(TARGET).pkg
	rm -rf pkg

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: $(OFILES)
	$(LD) $(OFILES) $(LDFLAGS) -o $@

%.self: %.elf
	$(MAKE_SELF) $< $@ $(CONTENTID)

pkg: $(TARGET).pkg

$(TARGET).pkg: $(TARGET).self
	@rm -rf pkg
	@mkdir -p pkg/USRDIR
	@cp $(ICON0) pkg/ICON0.PNG
	@cp $(TARGET).self pkg/USRDIR/EBOOT.BIN
	$(MAKE_SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) pkg/PARAM.SFO
	$(MAKE_PKG) --contentid $(CONTENTID) pkg/ $@
	@rm -rf pkg

.PHONY: default clean pkg
