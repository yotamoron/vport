
SUBDIRS = kernel user
MAKE = $(shell which make)

all:
	set -e; for d in ${SUBDIRS}; do CURR_DIR=${PWD}/$$d ${MAKE} -C $$d; done

clean:
	set -e; for d in ${SUBDIRS}; do CURR_DIR=${PWD}/$$d ${MAKE} -C $$d clean; done

