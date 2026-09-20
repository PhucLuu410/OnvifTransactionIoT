#ifndef ONVIF_TRANSACTION_H
#define ONVIF_TRANSACTION_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>

void OpenUdpSocketAndTransProbe(const char *Data, const char *Ip_addr, short Port);
void OpenTcpSockerAndTransSoap(const char *Data, const char *ip, short Port);

#endif