
include ../../core.mk

.NOTPARALLEL:
NAME=     jpeg
CATEGORY= graphics
DEST=     $(PARASOL_MODULE)
OBJECTS=  jpeg.o lib/jfdctfst.o lib/jfdctflt.o lib/jfdctint.o lib/jcapimin.o lib/jcapistd.o \
          lib/jccoefct.o lib/jcsample.o lib/jccolor.o lib/jcdctmgr.o lib/jchuff.o lib/jcinit.o \
          lib/jcmainct.o lib/jcmarker.o lib/jcmaster.o lib/jcprepct.o lib/jcomapi.o lib/jcparam.o \
          lib/jcphuff.o lib/jctrans.o lib/jdapimin.o \
          lib/jdapistd.o lib/jdatadst.o lib/jdatasrc.o lib/jdcoefct.o lib/jdcolor.o \
          lib/jddctmgr.o lib/jdhuff.o lib/jdinput.o lib/jdmainct.o lib/jdmarker.o \
          lib/jdmaster.o lib/jdmerge.o lib/jdphuff.o lib/jdpostct.o lib/jdsample.o \
          lib/jdtrans.o lib/jerror.o lib/jidctflt.o lib/jidctfst.o lib/jidctint.o \
          lib/jidctred.o lib/jmemansi.o lib/jmemmgr.o lib/jquant1.o lib/jquant2.o lib/jutils.o
CFLAGS += -DMOD_NAME=$(NAME)

compile: $(OBJECTS)
ifeq ($(OSTYPE),win)
	$(CC) $(LIBLINK) -lm $?
else
   ifeq ($(HOST),mac)
	$(CC) $(LIBLINK) -lm $?
   else
	$(TOOLCHAIN)ld -r -o lib.o $(OBJECTS)
#	$(OBJCOPY) -K ModHeader `sed 's/^/-L /' syms.exp` -R .comment -R .note --strip-unneeded lib.o
	$(OBJCOPY) -K ModHeader -R .comment -R .note --strip-unneeded lib.o
	$(CC) $(CFLAGS) -nostartfiles -shared -s -Wl,-soname,$(DEST),-version-script=version,-Bsymbolic -lm -o "$(DEST)" lib.o
	rm -f lib.o
   endif
endif

$(OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -Wno-main -c -o $@ $<

clean:
	@rm -f $(OBJECTS) $(DEST)
