
BUILD_ROOT ?= ../../
ZLIB=zlib-1.2.8

include $(BUILD_ROOT)makefile.inc

.NOTPARALLEL:

compile: libffi zlib

openssl:
	tar -zxvf openssl-1.0.*.gz
	cd openssl-1.0.*
	# Patch ssl/dtls1.h so that <sys/time.h> is included rather than <winsock.h>
	echo -e "#undef OPENSSL_SYS_WINDOWS\n#undef OPENSSL_SYS_WIN32\n$(cat include/openssl/dtls1.h)" >include/openssl/dtls11.h
	# Clear dtlstest.c because it doesn't compile in mingw
	echo "" >test/dtlstest.c
	$(MAKE) dclean
	./configure --prefix=/usr mingw no-ssl2 no-ec no-hw shared zlib-dynamic
	$(MAKE) depend && $(MAKE) all && $(MAKE) install

libffi:
	rm -fr libffi-3.2.1
	tar -zxvf libffi-3.2.1.tar.gz
	cd libffi-3.2.1; ./configure --disable-dependency-tracking --prefix=`pwd`/release --includedir=`pwd`/release/include; $(MAKE) install

zlib:
	rm -fr $(ZLIB)
	tar -zxvf $(ZLIB).tar.gz
	mkdir -p $(PARASOL_MODULES)/lib
ifeq ($(OSTYPE),win)
	BINARY_PATH=/mingw/bin INCLUDE_PATH=/mingw/include LIBRARY_PATH=/mingw/lib $(MAKE) -C $(ZLIB) clean install -f win32/Makefile.gcc
	cd $(ZLIB) && cp zlib1.dll $(PARASOL_MODULES)/lib/
else
	cd $(ZLIB) && ./configure && $(MAKE) && cp libz.so.1.2.8 $(PARASOL_MODULES)/lib/libz.so
endif

