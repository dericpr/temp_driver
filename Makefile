CC= /home/dpraymond/ti-sdk-am335x-evm-07.00.00.00/linux-devkit/sysroots/i686-arago-linux/usr/bin/arm-linux-gnueabihf-gcc --sysroot=/home/dpraymond/ti-sdk-am335x-evm-07.00.00.00/linux-devkit/sysroots/cortexa8hf-vfp-neon-3.8-oe-linux-gnueabi 
X86_CC=gcc
#/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-codesourcery-armle-fbdev-obj
LIB_PATH=./libs -L/home/dpraymond/linux-am335SDK-armle-fbdev-obj/lib -L/home/dpraymond/ti-sdk-am335x-evm-07.00.00.00/linux-devkit/sysroots/cortexa8hf-vfp-neon-3.8-oe-linux-gnueabi/usr/lib 
INC_PATH=/home/dpraymond/linux-am335SDK-armle-fbdev-obj/include -I/home/dpraymond/ti-sdk-am335x-evm-07.00.00.00/linux-devkit/sysroots/cortexa8hf-vfp-neon-3.8-oe-linux-gnueabi/usr/include -I./include

X86_LIB_PATH=/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-x86-fbdev-obj/lib
X86_INC_PATH=/Applications/Crank_Software/Storyboard_Engine/3.2.1.201405300145/linux-x86-fbdev-obj/include

LIBS = -lgreio -lpthread -lm  -lcurl
#gcc -g  -m32 -I/home/dpraymond/linux-x86-fbdev-obj/include/ -L/home/dpraymond/linux-x86-fbdev-obj/lib -o test main.c -lgreio -lpthread
all:
	${CC} -I${INC_PATH} -L${LIB_PATH} main.c -o temp_driver ${LIBS}

x86:
	${X86_CC} -m32 -I${X86_INC_PATH} -L${X86_LIB_PATH} main.c -o temp_driver_x86 ${LIBS}

clean:
	rm -f *.o temp_driver
