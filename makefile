CXX = g++

CXXFLAGS = -Wall -Wextra -g -std=c++11

INCLUDES = \
    -I. \
	-IOnvifXml \
	-IXmlParse \
	-INetwork \


SOURCES = \
    main.cpp \
	XmlParse/pugixml.cpp \
	Network/UdpSendProbe.cpp \
	Network/TcpSendSoap.cpp \

LIBS = \
    -lws2_32 \
    -lbcrypt

TARGET = main.exe


all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBS)


clean:
	rm -f $(TARGET)
	rm -f ProbeMatch_*.txt