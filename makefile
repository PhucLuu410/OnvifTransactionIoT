CXX = g++

CXXFLAGS = -Wall -Wextra -g -std=c++11

INCLUDES = \
    -I. \
	-IOnvifXml\


SOURCES = \
    main.cpp \


LIBS = \
    -lws2_32 \
    -lbcrypt

TARGET = main.exe


all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBS)


clean:
	rm -f $(TARGET)
	rm -f ProbeMatch_*.txt