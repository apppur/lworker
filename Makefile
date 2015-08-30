all :
	@echo Please do 'make PLATFORM' where PLATFORM is one of these : linux mingw

CFLAGS := -g -Wall
LDFLAGS := --shared

mingw : TARGET := lworker.dll
mingw : CFLAGS += -I/usr/local/include
mingw : LDFLAGS += -L/usr/local/bin -llua53

mingw : lworker

linux : TARGET := lworker.so
linux : CFLAGS += -I/usr/local/include
linux : LDFLAGS += -fPIC

linux : lworker

lworker: lworker.c
	gcc -o $(TARGET) $(CFLAGS) $^ $(LDFLAGS) -lpthread

clean :
	rm -f lworker.dll lworker.so
