subdirs := DOCUMENTS OFFICIAL PLAYER VIEWER EDITOR SAMPLES MYSOCKET
port0=7000
port1=7777
.PHONY: all $(subdirs)
all:  $(subdirs)

MYSOCKET:
	cd socket; make

my:
	official/official samples/sample.crs socket/server \
	${port0} socket/server ${port1} 

myf:
	official/official samples/sample.crs socket/server \
	${port0} socket/server ${port1} > ./test.racelog

OFFICIAL:
	cd official; make

PLAYER:
	cd player; make

DOCUMENTS:

VIEWER:

EDITOR:

SAMPLES:

tags:
	etags */*.hpp */*.cpp */*.js */*.html

tar: distclean
	tar zcvf ../samurai-software-`date -Iminutes` *

clean:
	rm -rf *~ */*~ \#*\#
	cd documents; make clean
	cd official; make clean
	cd player; make clean
	cd viewer; make clean
	cd editor; make clean
	cd samples; make clean
	cd socket; make clean

distclean:
	rm -rf *~ */*~ \#*\# TAGS
	cd documents; make distclean
	cd official; make distclean
	cd player; make distclean
	cd viewer; make distclean
	cd editor; make distclean
	cd samples; make distclean
