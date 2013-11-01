# Andrew Huang <bluedrum@163.com>
CC = gcc
AR = ar rcv
STRIP = strip
ifeq ($(windir),)
EXE =
RM = rm -f
else
EXE = .exe
RM = del
endif



all:libmincrypt.a mkbootimg$(EXE) unpackbootimg$(EXE)

libmincrypt.a:
	make -C libmincrypt

mkbootimg$(EXE):mkbootimg.o
	$(CC) -g -o $@ $^ -L. -lmincrypt
	$(STRIP) $@

mkbootimg.o:mkbootimg.c
	$(CC) -g -o $@ -c $< -I.


unpackbootimg$(EXE):unpackbootimg.o
	$(CC) -g -o $@ $^
	$(STRIP) $@

unpackbootimg.o:unpackbootimg.c
	$(CC) -g -o $@ -c $< 

clean:
	$(RM) mkbootimg mkbootimg.o unpackbootimg unpackbootimg.o 
	$(RM) libmincrypt.a Makefile.~
	make -C libmincrypt clean


		


	
