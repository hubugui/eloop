ifeq ($(OS),Windows_NT)
    detected_OS := win32
else
    detected_OS := $(shell uname)
endif

ifeq ($(detected_OS),win32)
    CFLAGS += -D WIN32
    DEPEND_LIBS := -lws2_32
endif

ifeq ($(detected_OS),Linux)
    CFLAGS   +=   -D LINUX
endif

ifeq ($(detected_OS),Darwin)
	CFLAGS   +=   -D OSX
endif

AR=ar
CC=gcc
DEL=rm -rf
MK=mkdir
OUT=objs
RM=rmdir /s /q
C_INCLUDE+=src
CFLAGS+=-O2 -m64 -Wall -Wno-incompatible-pointer-types -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-int-conversion
CFLAGS+=-g -I $(C_INCLUDE)
SRCS=$(wildcard src/buffer_pipe.c src/event_loop.c src/event_loop_pool.c src/event_channel.c src/event_channel_map.c)
ifeq ($(detected_OS),Darwin)
SRCS+=$(wildcard src/event_io_kqueue.c)
else
SRCS+=$(wildcard src/event_io_select.c)
endif
SRCS+=$(wildcard src/net/*.c)
SRCS+=$(wildcard src/common/*.c)
OBJS=$(SRCS:.c=.o)
TARGET=$(notdir $(CURDIR))
LIBS=lib$(TARGET).a
SUB_DIR = src

all: pingpong

$(TARGET): $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^
	-$(DEL) *.a

$(LIBS): $(OBJS)
	$(AR) -r $@ $^
	-$(DEL) *.o

pingpong: pingpong.c $(LIBS)
	$(CC) $^ $(CFLAGS) -o $@ $(DEPEND_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-$(DEL) $(OBJS)	
	-$(DEL) *.a
	-$(DEL) *.out
	-$(DEL) *.lib
	-$(DEL) *.exe

rebuild: clean all