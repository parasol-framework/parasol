
include ../../core.mk

NAME=     openssl
CATEGORY= network
DEST=     $(PARASOL_MODULE)
CFLAGS += -DMOD_NAME=$(NAME)

SSL_INCLUDE := $(wildcard ../../3rdparty/openssl-1.0.*/include)
LIBLINK += -L$(subst /include,,$(SSL_INCLUDE))

#-lssl32 -leay32 -lcrypto

compile:
ifeq ($(OSTYPE),win)
	$(CC) $(CFLAGS) -I$(SSL_INCLUDE) $(LIBLINK) $(NAME).c -lssl -lcrypto -lgdi32 -lwsock32
else
	$(CC) $(CFLAGS) -I$(SSL_INCLUDE) $(LIBLINK) $(NAME).c -lssl -lcrypto
endif

clean:
	@rm -f "$(DEST)"
