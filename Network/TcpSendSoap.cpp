#include "OnvifTransaction.h"
#include <cstdio>

void OpenTcpSockerAndTransSoap(const char *Data, const char *ip, short Port)
{
    SOCKET TcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (TcpSock == INVALID_SOCKET)
    {
        printf("Cannot build Tcp socket");
    }

    sockaddr_in ServerAddr = {};
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Port);

    inet_pton(AF_INET, ip, &ServerAddr.sin_addr);
    connect(TcpSock, (sockaddr *)&ServerAddr, sizeof(ServerAddr));
    send(TcpSock, Data, strlen(Data), 0);

    closesocket(TcpSock);
}