# LINUX:
#   ./configure
#   make

include ../../core.mk

.NOTPARALLEL:
NAME=    network
DEST=    $(PARASOL_ROOTMOD)
CFLAGS+= -Wall -Idns -DMAX_ALIASES=64 -DMAX_ADDRESSES=32 -DMOD_NAME=$(NAME)
DOCUMENTS= ./ netsocket/netsocket.c
HEADER= ../../include/parasol/modules/$(NAME).h

ARES := ares-1.11.0

ifeq ($(OSTYPE),win)
   CFLAGS    += -I$(SDK_ROOT)/core/3rdparty/openssl-*/include -I$(ARES)/
   OBJECTS   := win32/winsockwrappers.o
   LINK_ARES := y
   CFLAGS    += -DUSE_ARES -DCARES_STATICLIB
else
ifeq ($(OSTYPE),linux)
   LINK_ARES := y
   CFLAGS    +=  -DUSE_ARES -DCARES_STATICLIB -I$(ARES)/
endif
endif

ifeq ($(LINK_ARES),y)
DNS+= $(ARES)/ares_cancel.o         $(ARES)/ares_parse_mx_reply.o  $(ARES)/ares__close_sockets.o    $(ARES)/ares_parse_naptr_reply.o \
      $(ARES)/ares_create_query.o   $(ARES)/ares_parse_ns_reply.o  $(ARES)/ares_data.o              $(ARES)/ares_parse_ptr_reply.o \
      $(ARES)/ares_destroy.o        $(ARES)/ares_parse_soa_reply.o $(ARES)/ares_expand_name.o       $(ARES)/ares_parse_srv_reply.o \
      $(ARES)/ares_expand_string.o  $(ARES)/ares_parse_txt_reply.o $(ARES)/ares_fds.o               $(ARES)/ares_platform.o \
      $(ARES)/ares_free_hostent.o   $(ARES)/ares_process.o         $(ARES)/ares_free_string.o       $(ARES)/ares_query.o \
      $(ARES)/ares_getenv.o         $(ARES)/ares__read_line.o      $(ARES)/ares_gethostbyaddr.o     $(ARES)/ares_search.o \
      $(ARES)/ares_gethostbyname.o  $(ARES)/ares_send.o            $(ARES)/ares__get_hostent.o      $(ARES)/ares_strcasecmp.o \
      $(ARES)/ares_getnameinfo.o    $(ARES)/ares_strdup.o          $(ARES)/ares_getsock.o           $(ARES)/ares_strerror.o \
      $(ARES)/ares_init.o           $(ARES)/ares_timeout.o         $(ARES)/ares_library_init.o      $(ARES)/ares__timeval.o \
      $(ARES)/ares_llist.o          $(ARES)/ares_version.o         $(ARES)/ares_mkquery.o           $(ARES)/ares_writev.o \
      $(ARES)/ares_nowarn.o         $(ARES)/bitncmp.o              $(ARES)/ares_options.o           $(ARES)/inet_net_pton.o \
      $(ARES)/ares_parse_aaaa_reply.o  $(ARES)/inet_ntop.o         $(ARES)/ares_parse_a_reply.o     $(ARES)/windows_port.o
endif

OBJECTS+= $(NAME).o

compile: $(HEADER) $(OBJECTS) $(DNS) $(SSL_OBJ)
ifeq ($(OSTYPE),win)
	$(CC) $(CFLAGS) -shared $(LINK) -o "$(DEST)" $? -lwsock32
else
	$(CC) $(CFLAGS) -nostartfiles -shared $(LINK) -Wl,-soname,$(LIBNAME)$(LIBEXT),-version-script=version -o $(DEST) $?
endif

.FORCE:
test: .FORCE
	$(FLUTE) file=tests/dns-parallel.fluid
	$(FLUTE) file=tests/server-io.fluid
	$(FLUTE) file=tests/client-server.fluid

win32/winsockwrappers.o: win32/winsockwrappers.c makefile
	$(CC) $(CFLAGS) -c win32/winsockwrappers.c -o $@

$(NAME).o: $(NAME).c ssl.c netsocket/*.c class_proxy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(DNS): %.o: %.c makefile
	$(CC) $(CFLAGS) -I. -c -o $@ $<

$(HEADER) module_def.c: $(NAME).fdl $(NAME).c ssl.c
	$(IDL_C) --src=$< --files={ $(NAME).c } --output-defs=module_def.c --output-proto=module_def.c --prototypes=static
	$(IDL_DEF) --src=$< --append --output=module_def.c --format=c

clean:
	@rm -f $(DEST) $(OBJECTS) $(DNS)
