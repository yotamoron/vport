
obj-m += kvport.o

KERNEL_SOURCES = $(shell echo /usr/src/kernels/`uname -r`/)

all:
	make -C ${KERNEL_SOURCES} SUBDIRS=${CURR_DIR} modules

clean:
	make -C ${KERNEL_SOURCES} SUBDIRS=${CURR_DIR} clean

