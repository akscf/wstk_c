PREFIX ?= ./

UNAME=$(shell uname -s)
SO_LIB=libwstk.so
SO_LIB_ENABLE=true
ST_LIB=libwstk.a
ST_LIB_ENABLE=true

#DBG_FLAGS=-DWSTK_DEBUG__MEM -DWSTK_DEBUG__MT -DWSTK_DEBUG__DS
#DBG_FLAGS=-DWSTK_DEBUG__NET -DWSTK_DEBUG__DS

#BLD_FLAGS=-DWSTK_SHARED -DWSTK_BUILD

INC=-I./include
CFLAGS=-g -O2 
LD_FLAGS=-MT
LIBS=-lc -lm
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)

LIB_SOURCES=

LIB_SOURCES_CORE=./src/ezxml.c ./src/cJSON.c ./src/cJSON_Utils.c ./src/multipartparser.c
LIB_SOURCES_CORE+=./src/wstk-core.c ./src/wstk-common.c ./src/wstk-daemon.c ./src/wstk-mem.c ./src/wstk-str.c ./src/wstk-pl.c ./src/wstk-mbuf.c ./src/wstk-rand.c ./src/wstk-time.c ./src/wstk-regex.c ./src/wstk-pid.c 
LIB_SOURCES_CORE+=./src/wstk-file.c ./src/wstk-dir.c ./src/wstk-tmp.c ./src/wstk-uuid.c ./src/wstk-base64.c ./src/wstk-sha1.c ./src/wstk-md5.c ./src/wstk-crc32.c ./src/wstk-fmt.c ./src/wstk-uri.c ./src/wstk-escape.c ./src/wstk-endian.c
LIB_SOURCES_CORE+=./src/wstk-list.c ./src/wstk-hashtable.c ./src/wstk-queue.c ./src/wstk-worker.c ./src/wstk-log.c ./src/wstk-codepage.c

LIB_SOURCES_NET=./src/wstk-poll.c ./src/wstk-poll-select.c ./src/wstk-poll-epoll.c ./src/wstk-poll-kqueue.c
LIB_SOURCES_NET+=./src/wstk-net-util.c ./src/wstk-net-sa.c ./src/wstk-net-sock.c ./src/wstk-net-udp.c ./src/wstk-net-tcp.c
LIB_SOURCES_NET+=./src/wstk-udp-srv.c ./src/wstk-tcp-srv.c 

LIB_SOURCES_WEB=./src/wstk-websock.c ./src/wstk-http-msg.c
LIB_SOURCES_WEB+=./src/wstk-httpd.c ./src/wstk-httpd-utils.c ./src/wstk-servlet-jsonrpc.c ./src/wstk-servlet-websock.c ./src/wstk-servlet-upload.c

LIB_SOURCES_SSL=./src/wstk-ssl.c

#SSL_CFLAGS=-DWSTK_USE_SSL
#SSL_LIBS=-lssl

## Linux gcc-11.x
ifeq ($(UNAME),Linux) 
	CC=gcc
	LD=gcc
	AR=ar rcs
	MAKE=make
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_LINUX $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## FreeBSD clang-10.x
ifeq ($(UNAME),FreeBSD)
	CC=cc
	LD=cc
	AR=ar rcs
	MAKE=gmake
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lexecinfo -lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_FREEBSD $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## OpenBSD clang-16.x
ifeq ($(UNAME),OpenBSD)
	CC=cc
	LD=cc
	AR=ar rcs
	MAKE=gmake
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lexecinfo -lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_OPENBSD $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## NetBSD clang-x.x
ifeq ($(UNAME),NetBSD)
	CC=cc
	LD=cc
	AR=ar rcs
	MAKE=gmake
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_NETBSD $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## Darwin (not tested)
ifeq ($(UNAME),Darwin)
	CC=cc
	LD=cc
	AR=ar rcs
	MAKE=gmake
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_DARWIN $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## Solaris10 gcc-5x (OpenCSW)
ifeq ($(UNAME),SunOS)
	CC=gcc
	LD=gcc
	AR=ar rcs
	MAKE=gmake
	LIB_SOURCES_OS=./src/wstk-mutex-nix.c ./src/wstk-thread-nix.c ./src/wstk-sleep-nix.c ./src/wstk-net-init-nix.c ./src/wstk-dlo-nix.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lnsl -lsocket -lpthread $(SSL_LIBS)
	CFLAGS+=-fPIC -DWSTK_OS_SUNOS $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=-shared
endif

## OS/2 emx-gcc-3.x
ifeq ($(UNAME),OS/2) 
	CC=gcc
	LD=gcc
	AR=ar rcs
	MAKE=make	
	LIB_SOURCES_OS=./src/wstk-mutex-os2.c ./src/wstk-thread-os2.c ./src/wstk-sleep-os2.c ./src/wstk-net-init-os2.c ./src/wstk-dlo-os2.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS+=-lsocket $(SSL_LIBS)
	CFLAGS+=-std=c99 -DWSTK_OS_OS2 $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=
	SO_LIB_ENABLE=false
endif

## Windows MinGW 2.x
ifeq ($(UNAME),MINGW32_NT-5.2)
	CC=gcc
	LD=gcc
	AR=ar rcs
	MAKE=make
	LIB_SOURCES_OS=./src/wstk-mutex-win.c ./src/wstk-thread-win.c ./src/wstk-sleep-win.c ./src/wstk-net-init-win.c ./src/wstk-dlo-win.c
	LIB_SOURCES=$(LIB_SOURCES_CORE) $(LIB_SOURCES_NET) $(LIB_SOURCES_WEB) $(LIB_SOURCES_SSL) $(LIB_SOURCES_OS)
	LIBS=-lws2_32 -lwsock32 -ladvapi32 $(SSL_LIBS)
	CFLAGS+=-std=c99 -DWSTK_OS_WIN $(BLD_FLAGS) $(DBG_FLAGS) $(SSL_CFLAGS) $(INC)
	LD_FLAGS=
	SO_LIB_ENABLE=false
endif



all: $(LIB_SOURCES) $(SO_LIB) $(ST_LIB)

$(SO_LIB): $(LIB_OBJECTS)
ifeq ($(SO_LIB_ENABLE),true)
	$(LD) $(LD_FLAGS) -o $@ $(LIB_OBJECTS) $(LIBS)
endif

$(ST_LIB): $(LIB_OBJECTS)
ifeq ($(ST_LIB_ENABLE),true)
	$(AR) $@ $(LIB_OBJECTS)
endif

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

install:
	exit 0

clean:
	@rm -f src/*.o examples/*.o examples/*.bin examples/*.exe
	@rm -f *.so *.a core.*
