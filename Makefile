#!/usr/bin/make -f
# This is a pretty lazy Makefile, I don't care.
CROSS_COMPILE?=arm-none-linux-gnueabi-
NVCRYPTTOOLS_PATH?=../../nvcrypttools/

# i386 target info
TARGET=ptpatcher
LIBS=-lcrypto++
OBJS=aes-cmac.o
CC=gcc
CPP=g++
CFLAGS=

# arm target info
TARGET_ARM=ptpatcher.arm
OBJS_ARM=$(NVCRYPTTOOLS_PATH)nvaes.o
LIBS_ARM=
CC_ARM=$(CROSS_COMPILE)$(CC)
CFLAGS_ARM=-O2 -static -march=armv7-a -mthumb -I$(NVCRYPTTOOLS_PATH)

%.o: %.cpp
	$(CPP) -c -o $@ $<

default: $(TARGET)

all: $(TARGET) $(TARGET_ARM)

$(TARGET): $(OBJS) $(TARGET).c
	$(CPP) $(CFLAGS) -o $@ $(OBJS) $(TARGET).c $(LIBS)

$(TARGET_ARM): $(OBJS_ARM) $(TARGET).c
	$(CC_ARM) $(CFLAGS_ARM) -o $@ $(OBJS_ARM) $(TARGET).c $(LIBS_ARM)

clean:
	rm -f $(OBJS) $(TARGET) $(OBJS_ARM) $(TARGET_ARM)

.PHONY: default all clean
