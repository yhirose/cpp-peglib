
USE_CLANG = 1

ifdef USE_CLANG
CC = clang++
CFLAGS = -std=c++1y -stdlib=libc++ -g
else
CC = g++
CFLAGS = -std=c++1y -g
endif

all: calc calc2 calc3 pl0

calc : calc.cc ../peglib.h
	$(CC) -o calc $(CFLAGS) -I.. calc.cc

calc2 : calc2.cc ../peglib.h
	$(CC) -o calc2 $(CFLAGS) -I.. calc2.cc

calc3 : calc3.cc ../peglib.h
	$(CC) -o calc3 $(CFLAGS) -I.. calc3.cc

pl0 : pl0.cc ../peglib.h
	$(CC) -o pl0 $(CFLAGS) -I.. pl0.cc
