NAME = putpixel

MSXBASE = ../msxsys
CFLAGS = -mz80 -I$(MSXBASE)
LDFLAGS = --code-loc 0x170 --data-loc 0 --no-std-crt0\
	$(MSXBASE)/crt0msx_msxdos_advanced.o $(MSXBASE)/ioport.o $(MSXBASE)/interrupt.o $(MSXBASE)/msxlib.o\
	$(MSXBASE)/putchar.o
REL = $(NAME).rel 

$(NAME).com: $(REL)
	sdcc $(CFLAGS) $(REL) $(LDFLAGS)
	hex2bin -e com $(NAME).ihx

%.rel: %.c
	sdcc $(CFLAGS) -c $<

$(NAME).dsk: $(NAME).com 
	-rm $@
	wrdsk $@ $(MSXBASE)/msxdos.sys $(MSXBASE)/command.com \
	         $<

run: $(NAME).dsk
	openmsx -machine Philips_NMS_8250 -diska $(NAME).dsk

clean:
	-rm *.lnk *.lst *.asm *.map *.o *.ihx *.rel as.com *~ *.sym *.dsk
