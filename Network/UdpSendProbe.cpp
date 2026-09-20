#include "OnvifTransaction.h"
#include <cstdio>

void OpenUdpSocketAndTransProbe(const char *Data, const char *Ip_addr, short Port)
{
    FILE *file = fopen("ProbeMatch.xml", "a");

    WSADATA wsaData;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET UdpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (UdpSock == INVALID_SOCKET)
    {
        printf(
            "socket() failed: %d\n",
            WSAGetLastError());

        WSACleanup();
        return;
    }

    sockaddr_in localAddr = {};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(Port);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    bind(UdpSock, (sockaddr *)&localAddr, sizeof(localAddr));

    in_addr localInterface = {};
    inet_pton(AF_INET, Ip_addr, &localInterface);
    setsockopt(UdpSock, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&localInterface, sizeof(localInterface));
    DWORD timeout = 5000;
    setsockopt(UdpSock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

    sockaddr_in multicastAddr = {};
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(3702);

    inet_pton(AF_INET, "239.255.255.250", &multicastAddr.sin_addr);

    sendto(UdpSock, Data, strlen(Data), 0, (sockaddr *)&multicastAddr, sizeof(multicastAddr));

    char buffer[8192];

    printf("========================================\n");
    printf("Start Scan\n");
    printf("========================================\n");

    int Count = 0;
    while (Count < 3)
    {
        sockaddr_in senderAddr = {};
        int senderAddrLen = sizeof(senderAddr);

        int received = recvfrom(UdpSock, buffer, sizeof(buffer) - 1, 0, (sockaddr *)&senderAddr, &senderAddrLen);

        buffer[received] = '\0';

        fprintf(file, "========================================\n");

        fprintf(file, "From: %s\n", inet_ntoa(senderAddr.sin_addr));

        fprintf(file, "Received: %d bytes\n", received);

        fprintf(file, "========================================\n");

        fprintf(file, "%s\n", buffer);

        fflush(file);

        Count++;
    }

    printf("========================================\n");
    printf("Scan Done\n");
    printf("========================================\n");

    fclose(file);
    closesocket(UdpSock);
}