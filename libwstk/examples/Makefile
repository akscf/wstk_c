#
# usage: make TNAME=uuid or something else
#
UNAME=$(shell uname -s)

SO_LIB_ENABLE=false
ST_LIB_ENABLE=true
ST_LIB=-L../ ../libwstk.a
SO_LIB=-L../ -lwstk

INC=-I../include
CFLAGS=-g -O2
LD_FLAGS=
LIBS=-lc -lm

#SSL_CFLAGS=-DWSTK_USE_SSL
#SSL_LIBS=-lssl

SRC=test-$(TNAME).c
OBJ=test-$(TNAME).o

ifeq ($(UNAME),Linux) 
	CC=gcc
	LD=gcc
	MAKE=make
	LIBS=-lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_LINUX $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),FreeBSD)
	CC=cc
	LD=cc
	MAKE=gmake
	LIBS+=-lexecinfo -lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_FREEBSD $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),OpenBSD)
	CC=cc
	LD=cc
	MAKE=gmake
	LIBS+=-lexecinfo -lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_OPENBSD $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),NetBSD)
	CC=cc
	LD=cc
	MAKE=gmake
	LIBS+=-lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_NETBSD $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),Darwin)
	CC=cc
	LD=cc
	MAKE=gmake
	LIBS+=-lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_DARWIN $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),SunOS)
	CC=gcc
	LD=gcc
	MAKE=gmake
	LIBS+=-lnsl -lsocket -lrt -lpthread $(SSL_LIBS)
	CFLAGS+=-DWSTK_OS_SUNOS -DWSTK_BUILTIN_FP_CONST $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).bin
endif

ifeq ($(UNAME),OS/2) 
	CC=gcc
	LD=gcc
	MAKE=make	
	LIBS+=-lsocket $(SSL_LIBS)
	CFLAGS+=-std=c99 -DWSTK_OS_OS2 -DWSTK_BUILTIN_FP_CONST $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-Zmt
	BIN_OUT=test-$(TNAME).exe
	SO_LIB_ENABLE=false
endif

ifeq ($(UNAME),MINGW32_NT-5.2)
	CC=gcc
	LD=gcc
	MAKE=make	
	LIBS=-lws2_32 -lwsock32 -ladvapi32 $(SSL_LIBS)
	CFLAGS+=-std=c99 -DWSTK_OS_WIN $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-MT
	BIN_OUT=test-$(TNAME).exe
	SO_LIB_ENABLE=false
endif


all: build
build: $(SRC) $(BIN_OUT)

$(BIN_OUT): $(OBJ)
ifeq ($(SO_LIB_ENABLE),true)
	$(LD) -o $@ $(OBJ) $(LD_FLAGS) $(SO_LIB) $(LIBS)
else
	$(LD) -o $@ $(OBJ) $(LD_FLAGS) $(ST_LIB) $(LIBS) 
endif

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
    
clean:
	@rm -f *.o *.bin *.exe
