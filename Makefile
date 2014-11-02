CC=/usr/local/carlson-minot/crosscompilers/bin/arm-none-linux-gnueabi-gcc
#/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-codesourcery-armle-fbdev-obj
LIB_PATH=/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-codesourcery-armle-fbdev-obj/lib
INC_PATH=/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-codesourcery-armle-fbdev-obj/include

LIBS = -lgreio -lpthread
#gcc -g  -m32 -I/home/dpraymond/linux-x86-fbdev-obj/include/ -L/home/dpraymond/linux-x86-fbdev-obj/lib -o test main.c -lgreio -lpthread
all:
	${CC} -I${INC_PATH} -L${LIB_PATH} main.c -o temp_driver ${LIBS}
