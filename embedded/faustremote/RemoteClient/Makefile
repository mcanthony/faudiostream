
sources =  $(wildcard Sources/*.cpp)

objects = $(sources:.cpp=.o)

os 	:= $(shell uname)
system	?= $(shell uname -s)

PREFIX ?= /usr/local

ifeq ($(system), Darwin)
	CXX 		= g++
	CXXFLAGS	= -arch i386 -arch x86_64 -Wglobal-constructors  
	LIB_EXT        = dylib
else 
	ARCHFLAGS 	:= 
	LIBFLAGS	:= --mode=link --tag=CXX
	LIB_EXT        = so
endif

first: all

####### Build rules

all: $(objects) libfaustremote.a


libfaustremote.a : $(objects)
		libtool -static $(LIBFLAGS) $(objects) -o libfaustremote.a 

install:
	install libfaustremote.a  /usr/local/lib/faust/ 
	cp remote-dsp.h  /usr/local/include/faust/

clean :
	rm -f $(objects) libfaustremote.a

remote_dsp_aux.o: JsonParser.h

