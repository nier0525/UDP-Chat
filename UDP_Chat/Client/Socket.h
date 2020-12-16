#pragma once

#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "Global.h"

void err_quit(const char* msg);
void err_display(const char* msg);

int recvn(SOCKET sock, char* buf, int len, int flags);
int memcpy_send(void* ptr, const void* dst, size_t size, int* len);
int memcpy_recv(void* dst, const void* ptr, size_t size);
PROTOCOL GetProtocol(char* buf);
bool packing_recvn(SOCKET s, char* buf);

int Packing(char* buf, const PROTOCOL protocol);
int Packing(char* buf, const PROTOCOL protocol, const int num);
int Packing(char* buf, const PROTOCOL protocol, const char* name);

void UnPacking(const char* buf, ROOMINFO& data);
void UnPacking(const char* buf, char* ip, int& count);
void UnPacking(const char* buf, char* msg);

void TCP_SocketInit(SOCKET& sock, SOCKADDR_IN& addr);
void UDP_SocketInit(SOCKET& sock, SOCKADDR_IN& addr, int ttl);

bool TCP_Connect(Client_Info* ptr);
void UDP_Address_Setup(SOCKADDR_IN& addr, const char* ip);
bool Multicast_Group_Add(SOCKET& sock, ip_mreq& mreq, const char* ip);
void Multicast_Group_Drop(SOCKET& sock, ip_mreq mreq);

void CleanSocket(Client_Info* ptr);