#include <stdio.h>
#include <string.h>
#include "OnvifTransaction.h"
#include "pugixml.hpp"
#include <iostream>
#include "Probe.h"

int main(void)
{
    printf("1");
    OpenUdpSocketAndTransProbe(ProbeMessage, "192.168.1.153", 9999);

    printf("1");
    pugi::xml_document doc;
    doc.load_file("ProbeMatch.xml");
    pugi::xml_node envelope = doc.child("SOAP-ENV:Envelope");
    pugi::xml_node body = envelope.child("SOAP-ENV:Body");
    pugi::xml_node probematches = body.child("d:ProbeMatches");
    pugi::xml_node probematch = probematches.child("d:ProbeMatch");
    pugi::xml_node xaddrs = probematch.child("d:XAddrs");

    std::string xaddr = xaddrs.child_value();

    size_t start = xaddr.find_first_not_of(" \t\r\n");
    size_t end = xaddr.find_last_not_of(" \t\r\n");

    if (start != std::string::npos)
    {
        xaddr = xaddr.substr(start, end - start + 1);
    }

    const char *ptr = xaddr.c_str();

    printf("%s\n", ptr);

    OpenTcpSockerAndTransSoap(GetDateAndTime, "192.168.1.200", 2020);

    WSACleanup();

    return 0;
}