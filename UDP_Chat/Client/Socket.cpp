#include "Socket.h"

// ���� �Լ� ���� ���
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	ExitProcess(0);
}

void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(nullptr, (char*)lpMsgBuf, msg, MB_OK);
	LocalFree(lpMsgBuf);
}

int recvn(SOCKET sock, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0)
	{
		received = recv(sock, ptr, left, flags);
		if (received == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}
		else if (received == 0)
		{
			break;
		}

		left -= received;
		ptr += received;
	}

	return (len - left);
}

// ���� ���� ��ŷ �Լ�
int memcpy_send(void* ptr, const void* dst, size_t size, int* len) {
	memcpy(ptr, dst, size);	// �޸� ����
	*len += size;						// �޸��� ũ�⸦ ���ڰ����� ���� ũ�⸸ŭ ����
	return size;						// ���ڰ����� ���� ũ�� ����
}

// ���� ���� ����ŷ �Լ�
int memcpy_recv(void* dst, const void* ptr, size_t size) {
	memcpy(dst, ptr, size);	// �޸� ����
	return size;						// ���ڰ����� ���� ũ�� ����
}

PROTOCOL GetProtocol(char* buf) {
	PROTOCOL protocol;
	memcpy(&protocol, buf, sizeof(PROTOCOL));

	return protocol;
}

bool packing_recvn(SOCKET s, char* buf) {
	int retval;
	int size;

	retval = recvn(s, (char*)&size, sizeof(int), 0);
	if (retval == SOCKET_ERROR) {
		err_display("recvn");
		return false;
	}
	else if (retval == 0) {
		return false;
	}

	// ���� ����
	retval = recvn(s, buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("recvn");
		return false;
	}
	else if (retval == 0) {
		return false;
	}

	return true;
}

// Packing
int Packing(char* buf, const PROTOCOL protocol) {
	int size = 0;
	char* ptr = buf + sizeof(int);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

int Packing(char* buf, const PROTOCOL protocol, const int num) {
	int size = 0;
	char* ptr = buf + sizeof(int);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr += memcpy_send(ptr, &num, sizeof(int), &size);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

int Packing(char* buf, const PROTOCOL protocol, const char* name) {
	int size = 0;
	char* ptr = buf + sizeof(int);
	int len = strlen(name);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr += memcpy_send(ptr, &len, sizeof(int), &size);
	ptr += memcpy_send(ptr, name, len, &size);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

// UnPacking
void UnPacking(const char* buf, ROOMINFO& data) {
	const char* ptr = buf + sizeof(int);
	int len = sizeof(ROOMINFO);

	ptr += memcpy_recv(&len, ptr, sizeof(int));
	ptr += memcpy_recv(&data, ptr, len);
}

void UnPacking(const char* buf, char* ip, int& count) {
	const char* ptr = buf + sizeof(int);
	int len = 0;

	ptr += memcpy_recv(&len, ptr, sizeof(int));
	ptr += memcpy_recv(ip, ptr, len);
	ptr += memcpy_recv(&count, ptr, sizeof(int));
}

void UnPacking(const char* buf, char* msg) {
	const char* ptr = buf + sizeof(int);
	int len = 0;

	ptr += memcpy_recv(&len, ptr, sizeof(int));
	ptr += memcpy_recv(msg, ptr, len);
}

// TCP ���� �ʱ�ȭ
void TCP_SocketInit(SOCKET& sock, SOCKADDR_IN& addr) {
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		err_quit("socket()");
	}

	ZeroMemory(&addr, sizeof(SOCKADDR_IN));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(IP);
	addr.sin_port = htons(PORT);

	//int retval = connect(sock, (SOCKADDR*)&addr, sizeof(addr));
	//if (retval == SOCKET_ERROR) {
	//	err_quit("connect");
	//}
}

// TCP Ŀ��Ʈ
bool TCP_Connect(Client_Info* ptr) {
	int retval = connect(ptr->tcp_sock, (SOCKADDR*)&ptr->tcp_addr, sizeof(ptr->tcp_addr));
	if (retval == SOCKET_ERROR) {
		err_display("connect");
		return false;
	}
	return true;
}

// UDP ���� �ʱ�ȭ
void UDP_SocketInit(SOCKET& sock, SOCKADDR_IN& addr, int ttl) {
	int retval;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		err_quit("socket()");
	}

	// ��Ƽĳ��Ʈ TTL ����
	retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
		(char *)&ttl, sizeof(ttl));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	BOOL optval = TRUE;
	retval = setsockopt(sock, SOL_SOCKET,
		SO_REUSEADDR, (char *)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT + 1);
	retval = bind(sock, (SOCKADDR*)&addr, sizeof(addr));
	if (retval == SOCKET_ERROR) {
		err_quit("bind()");
	}
}

// UDP ��巹�� ����
void UDP_Address_Setup(SOCKADDR_IN& addr, const char* ip) {
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(PORT + 1);
}

// ��Ƽĳ��Ʈ �׷� ����
bool Multicast_Group_Add(SOCKET& sock, ip_mreq& mreq, const char* ip) {
	int retval;

	// ��Ƽĳ��Ʈ �׷� ����
	mreq.imr_multiaddr.s_addr = inet_addr(ip);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	retval = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) {
		err_display("setsockopt()");
		return false;
	}

	return true;
}

// ��Ƽĳ��Ʈ �׷� Ż��
void Multicast_Group_Drop(SOCKET& sock, ip_mreq mreq) {
	int retval;

	// ��Ƽĳ��Ʈ �׷� Ż��
	retval = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		(char *)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) {
		err_display("setsockopt()");
	}
}

// ���� ����
void CleanSocket(Client_Info* ptr) {
	closesocket(ptr->tcp_sock);
	closesocket(ptr->udp_sock);
	WSACleanup();
}