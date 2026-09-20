CXX = g++

CXXFLAGS = -Wall -Wextra -g -std=c++11 \
           -static-libgcc \
           -static-libstdc++

INCLUDES = \
	-I. \
	-IOnvifXml \
	-IXmlParse \
	-INetwork

SOURCES = \
	main.cpp \
	XmlParse/pugixml.cpp \
	Network/UdpSendProbe.cpp \
	Network/TcpSendSoap.cpp

LIBS = \
	-lws2_32 \
	-lbcrypt

TARGET = main.exe

all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
	del /Q $(TARGET) 2>nul
	del /Q ProbeMatch_*.txt 2>nul