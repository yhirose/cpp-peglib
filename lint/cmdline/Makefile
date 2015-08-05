
USE_CLANG = 1

ifdef USE_CLANG
CC = clang++
CFLAGS = -std=c++1y -stdlib=libc++ -g
else
CC = g++
CFLAGS = -std=c++1y -g
endif

all: peglint

peglint : peglint.cc ../peglib.h
	$(CC) -o peglint $(CFLAGS) -I../.. peglint.cc
