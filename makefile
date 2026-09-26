CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++11

INCLUDES = \
    -I. \
    -Inetwork/include \
    -Iauth/include \
    -Idiscovery/include \
    -Idevice/include \
    -Icamera_test/include \
    -Ixml/include

SOURCES = \
    main.cpp \
    xml/source/pugixml.cpp \
    network/source/net_utils.cpp \
    network/source/udp_socket.cpp \
    network/source/http_client.cpp \
    auth/source/sha1.cpp \
    auth/source/base64.cpp \
    auth/source/ws_security.cpp \
    discovery/source/camera_device.cpp \
    discovery/source/ws_discovery.cpp \
    device/source/onvif_device.cpp \
    camera_test/source/hls_streamer.cpp \
    camera_test/source/hls_server.cpp

LIBS = \
    -lws2_32 \
    -liphlpapi \
    -lbcrypt

TARGET = main.exe

all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
	rm -f *.o
	rm -rf hls