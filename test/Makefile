
USE_CLANG = 1

ifdef USE_CLANG
CC = clang++
CCFLAGS = -std=c++1y -stdlib=libc++ -g
else
CC = g++
CCFLAGS = -std=c++1y -g
endif

all : test
	./test

test : test.cc ../peglib.h
	$(CC) -o test $(CCFLAGS) -I.. -I. test.cc
