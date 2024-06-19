CC=g++
CXX=g++

CODESRC= thread.cpp uthreads.cpp
CODESRC_HEADERS= thread.h uthreads.h
EXEOBJ= thread.o uthreads.o

INCS=-I.
CFLAGS = -Wall -std=c++11 -O3 $(INCS)
CXXFLAGS = -Wall -std=c++11 -O3 $(INCS)

TARGET=libuthreads.a

all: $(TARGET)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex2.tar
TARSRCS=$(CODESRC) Makefile README thread.h uthreads.h thread.cpp uthreads.cpp

$(TARGET): $(EXEOBJ)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET) $(EXEOBJ)

depend:
	makedepend -- $(CFLAGS) -- $(CODESRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

.PHONY: all clean depend tar