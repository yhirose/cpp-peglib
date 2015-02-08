
CC = clang++
CCFLAGS = -std=c++11 -stdlib=libc++ -g

all : test
	./test

test : test.cc ../peglib.h
	$(CC) -o test $(CCFLAGS) -I.. -I. test.cc
