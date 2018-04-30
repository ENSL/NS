######################################################################
# Particle System API
#
# Copyright 1998 by David K. McAllister.
#
######################################################################

C++ = g++ -m32

GLUT_HOME  =/usr/local/contrib/unmoderated

MP = #-mp -DPARTICLE_MP

OS:=$(shell uname)

# Make it real fast on an Origin 2000.
#LNO =-LNO:opt=1:fusion=2:fission=2:fusion_peeling_limit=2048:cs1=32K:cs2=8M
COPT = $(MP) -O3 $(LNO)  -fPIC

CFLAGS = $(COPT) $(COMPFLAGS) -I. -I.. -I$(GLUT_HOME)/include -I/usr/include/c++/4.8/ -I../common -I../public -I../pm_shared -I../engine

ifeq ($(OS),Darwin)
SLIBB = $(C++) -dynamiclib -undefined suppress -flat_namespace *.o -o libparticles.dylib
CFLAGS += -target i386-apple-darwin -DCMAKE_OSX_ARCHITECTURES=i386
else
SLIBB = ar clq $@ $(POBJS)
endif

POBJS =action_api.o actions.o HLRender.o opengl.o system.o

ALL = libparticleMP.a

all: $(ALL)

# following line needed for c++ .cc files
.SUFFIXES	: .cpp

.cpp.o:
	$(C++) $(CFLAGS) -c $<

libparticleMP.a: $(POBJS)
	rm -f $@
	$(SLIBB)

SPDir:
	(make)

clean:
	rm -f libpar*
	rm -f *.o
	rm -rf *ii_files
