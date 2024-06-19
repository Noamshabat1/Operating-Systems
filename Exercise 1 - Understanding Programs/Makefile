# Compiler
CXX = g++

CXXFLAGS = -Wall -O2

TARGET = memory_latency

SRCS = measure.cpp memory_latency.cpp

HEADERS = measure.h memory_latency.h

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

tar:
	tar -cvf $(TARGET).tar.gz $(SRCS) $(HEADERS) Makefile
