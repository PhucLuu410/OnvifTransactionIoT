#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Probe.h"

int main(void)
{
    FILE *file = fopen("ProbeMatch.txt", "a");

    WSADATA wsaData;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET UdpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (UdpSock == INVALID_SOCKET)
    {
        printf(
            "socket() failed: %d\n",
            WSAGetLastError());

        WSACleanup();
        return 1;
    }

    sockaddr_in localAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(9999);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    bind(UdpSock, (sockaddr *)&localAddr, sizeof(localAddr));

    in_addr localInterface = {};
    inet_pton(AF_INET, "192.168.3.110", &localInterface);
    setsockopt(UdpSock, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&localInterface, sizeof(localInterface));

    sockaddr_in multicastAddr = {};
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(3702);

    inet_pton(AF_INET, "239.255.255.250", &multicastAddr.sin_addr);

    sendto(UdpSock, ProbeMessage, strlen(ProbeMessage), 0, (sockaddr *)&multicastAddr, sizeof(multicastAddr));

    char buffer[8192];

    while (1)
    {
        sockaddr_in senderAddr = {};
        int senderAddrLen = sizeof(senderAddr);

        int received = recvfrom(UdpSock, buffer, sizeof(buffer) - 1, 0, (sockaddr *)&senderAddr, &senderAddrLen);

        buffer[received] = '\0';

        fprintf(
            file,
            "========================================\n");

        fprintf(file, "From: %s\n", inet_ntoa(senderAddr.sin_addr));

        fprintf(file, "Received: %d bytes\n", received);

        fprintf(file, "========================================\n");

        fprintf(file, "%s\n", buffer);

        fflush(file);
    }

    fclose(file);

    SOCKET TcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (TcpSock == INVALID_SOCKET)
    {
        printf("Cannot build Tcp socket");
    }

    sockaddr_in ServerAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(2020);

    inet_pton(AF_INET, "192.168.1.200", &ServerAddr.sin_addr);
    connect(TcpSock, (sockaddr *)&ServerAddr, sizeof(ServerAddr));
    printf("TCP connected successfully\n");

    closesocket(TcpSock);
    closesocket(UdpSock);
    WSACleanup();

    return 0;
}