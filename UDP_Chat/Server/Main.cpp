#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9050
#define MULTICAST_IP_1 "225.0.0.1"
#define MULTICAST_IP_2 "225.0.0.2"
#define MULTICAST_IP_3 "225.0.0.3"

#define BUFSIZE    4096
#define MSGSIZE 128
#define MAX_CLIENT 100
#define ROOM_SIZE 10
#define MAX_SIZE 20

enum STATE {
	WAIT,
	VIEW,
	SELECT,
	ROOMIP,
	IN_ROOM,
	OUT_ROOM,
	EXIT
};

enum PROTOCOL {
	CALL,
	VIEWROOM,
	SEND_ROOMIP,
	QUIT
};

enum ROOM_NUM { A = 1, B, C };

struct Client_Info {
	SOCKET tcp_sock;
	SOCKADDR_IN tcp_addr;
	SOCKET udp_sock;
	SOCKADDR_IN udp_addr;

	char buf[BUFSIZE];
	STATE state;
};

struct User_Info {
	char name[MSGSIZE];
	char room_ip[MSGSIZE];
	int room_num;

	char chat[MSGSIZE];
	char msg[MSGSIZE];

	char room_user[MAX_SIZE][MSGSIZE];
	int room_count;
};

struct ROOMINFO {
	char name[ROOM_SIZE][MSGSIZE];
	char size;
};

Client_Info* client_list[MAX_CLIENT];
int client_count = 0;

char member_list[ROOM_SIZE][MAX_SIZE][MSGSIZE];
int user_count[ROOM_SIZE];

CRITICAL_SECTION cs;

void err_quit(const char* msg);
void err_display(const char* msg);

SOCKET listen_socket_init();
int recvn(SOCKET s, char* buf, int len, int flags);

int memcpy_send(void* ptr, const void* dst, size_t size, int* len);
int memcpy_recv(void* dst, const void* ptr, size_t size);

bool Packing_Recv(SOCKET sock, char* buf);
int GetProtocol(char* buf);

Client_Info* Add_Client(SOCKET sock, SOCKADDR_IN addr);
bool Remove_Client(Client_Info*);

SOCKET UDP_socket_init(int ttl);
SOCKADDR_IN UDP_Address_Setup(const char* ip);

void Add(int num, int count, char* data);
void Remove(int num, int count, char* data);

bool RoomSelect(int num, char* buf);

int Packing(char* buf, const int protocol, const char* msg);
int Packing(char* buf, const int protocol, const ROOMINFO data);

void UnPacking(const char* buf, int& num);
int UnPacking(const char* buf, char* msg);

DWORD WINAPI ClientThread(LPVOID arg);

// Main
int main() {
	int retval;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != NULL) return 1;

	SOCKET listen_sock = listen_socket_init();
	InitializeCriticalSection(&cs);

	ZeroMemory(&user_count, sizeof(user_count));

	SOCKET client_sock;
	SOCKADDR_IN client_addr;
	int addrlen;

	while (1) {
		addrlen = sizeof(client_addr);
		client_sock = accept(listen_sock, (SOCKADDR*)&client_addr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept");
			continue;
		}

		Client_Info* ptr = Add_Client(client_sock, client_addr);
		HANDLE hThread = CreateThread(nullptr, 0, ClientThread, (LPVOID)ptr, 0, nullptr);
		if (hThread == nullptr) {
			Remove_Client(ptr);
		}
		else {
			CloseHandle(hThread);
		}
	}

	closesocket(listen_sock);
	DeleteCriticalSection(&cs);
	WSACleanup();
	return 0;
}

void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

SOCKET listen_socket_init() {
	int retval;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr,
		sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	printf("Listen Socket Init\n");
	return listen_sock;
}

int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// 패킹 메모리 복사 함수
int memcpy_send(void* ptr, const void* dst, size_t size, int* len) {
	memcpy(ptr, dst, size);
	*len += size;
	return size;
}

// 언패킹 메모리 복사 함수
int memcpy_recv(void* dst, const void* ptr, size_t size) {
	memcpy(dst, ptr, size);
	return size;
}

// 패킹 Recv 함수
bool Packing_Recv(SOCKET sock, char* buf) {
	int retval;
	int size;

	retval = recvn(sock, (char*)&size, sizeof(int), 0);
	if (retval == SOCKET_ERROR) {
		err_display("recvn");
		return false;
	}
	else if (retval == 0) {
		return false;
	}

	retval = recvn(sock, buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("recvn");
		return false;
	}
	else if (retval == 0) {
		return false;
	}

	return true;
}

// 프로토콜 호출 함수
int GetProtocol(char* buf) {
	int protocol;
	memcpy(&protocol, buf, sizeof(int));
	return protocol;
}

Client_Info* Add_Client(SOCKET sock, SOCKADDR_IN addr) {
	EnterCriticalSection(&cs);

	Client_Info* ptr = new Client_Info();
	ZeroMemory(ptr, sizeof(Client_Info));

	ptr->tcp_sock = sock;
	memcpy(&ptr->tcp_addr, &addr, sizeof(SOCKADDR_IN));

	ptr->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

	printf("[ 클라이언트 접속 ] IP : %s , PORT : %d\n",
		inet_ntoa(ptr->tcp_addr.sin_addr), ntohs(ptr->tcp_addr.sin_port));

	ptr->state = WAIT;
	client_list[client_count++] = ptr;

	LeaveCriticalSection(&cs);
	return ptr;
}

bool Remove_Client(Client_Info* ptr) {
	EnterCriticalSection(&cs);

	printf("[ 클라이언트 종료 ] IP : %s , PORT : %d\n",
		inet_ntoa(ptr->tcp_addr.sin_addr), ntohs(ptr->tcp_addr.sin_port));
	closesocket(ptr->tcp_sock);
	closesocket(ptr->udp_sock);

	for (int i = 0; i < client_count; i++) {
		if (client_list[i] == ptr) {
			delete client_list[i];
			for (int j = i; j < client_count - 1; j++) {
				client_list[j] = client_list[j + 1];
			}
			ZeroMemory(client_list[client_count - 1], sizeof(Client_Info));
			client_count--;

			LeaveCriticalSection(&cs);
			return true;
		}
	}
	LeaveCriticalSection(&cs);
	return false;
}

SOCKET UDP_socket_init(int ttl) {
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		err_quit("socket");
	}

	// 멀티캐스트 TTL 설정
	int retval = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
		(char *)&ttl, sizeof(ttl));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	return sock;
}

SOCKADDR_IN UDP_Address_Setup(const char* ip) {
	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(SERVERPORT + 1);

	return addr;
}

void Add(int num, int count, char* data) {
	if (count < MAX_SIZE) {
		strcpy(member_list[num][count], data);
	}
}

void Remove(int num, int count, char* data) {
	char temp[MSGSIZE];
	for (int i = 0; i < count; i++) {
		if (!strcmp(member_list[num][i], data)) {
			for (int j = i; j < count - 1; j++) {
				strcpy(temp, member_list[num][j]);
				strcpy(member_list[num][j], member_list[num][j + 1]);
				strcpy(member_list[num][j + 1], temp);
			}
			break;
		}
	}	

	ZeroMemory(&member_list[num][count], sizeof(MSGSIZE));
}

bool RoomSelect(int num, char* buf) {
	switch (num) {
	case A:
		strcpy(buf, MULTICAST_IP_1);
		break;

	case B:
		strcpy(buf, MULTICAST_IP_2);
		break;

	case C:
		strcpy(buf, MULTICAST_IP_3);
		break;

	default:
		return false;
	}
	return true;
}

int Packing(char* buf, const int protocol, const char* msg) {
	int size = 0;
	char* ptr = buf + sizeof(int);
	int len = strlen(msg);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr += memcpy_send(ptr, &len, sizeof(int), &size);
	ptr += memcpy_send(ptr, msg, len, &size);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

int Packing(char* buf, const int protocol, const char* msg, const int num) {
	int size = 0;
	char* ptr = buf + sizeof(int);
	int len = strlen(msg);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr += memcpy_send(ptr, &len, sizeof(int), &size);
	ptr += memcpy_send(ptr, msg, len, &size);
	ptr += memcpy_send(ptr, &num, sizeof(int), &size);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

int Packing(char* buf, const int protocol, const ROOMINFO data) {
	int size = 0;
	char* ptr = buf + sizeof(int);
	int len = sizeof(ROOMINFO);

	ptr += memcpy_send(ptr, &protocol, sizeof(int), &size);
	ptr += memcpy_send(ptr, &len, sizeof(int), &size);
	ptr += memcpy_send(ptr, &data, len, &size);

	ptr = buf;
	memcpy(ptr, &size, sizeof(int));
	return size + sizeof(int);
}

void UnPacking(const char* buf, int& num) {
	const char* ptr = buf + sizeof(int);
	
	ptr += memcpy_recv(&num, ptr, sizeof(int));
}

int UnPacking(const char* buf, char* msg) {
	const char* ptr = buf + sizeof(int);
	int len = 0;

	ptr += memcpy_recv(&len, ptr, sizeof(int));
	ptr += memcpy_recv(msg, ptr, len);

	return len;
}

DWORD WINAPI ClientThread(LPVOID arg) {
	Client_Info* ptr = (Client_Info*)arg;

	int retval;
	int size;
	int num;

	char msg[MSGSIZE];
	char ip[MSGSIZE];
	char buf[BUFSIZE];
	
	bool flag = false;

	User_Info user;
	PROTOCOL protocol;

	ROOMINFO roominfo;
	ZeroMemory(&roominfo, sizeof(ROOMINFO));
	strcpy(roominfo.name[roominfo.size++], "박용택방");
	strcpy(roominfo.name[roominfo.size++], "오지환방");
	strcpy(roominfo.name[roominfo.size++], "이천웅방");

	SOCKET udp_sock = UDP_socket_init(2);
	SOCKADDR_IN udp_addr;

	while (!flag) {
		ZeroMemory(&ptr->buf, sizeof(BUFSIZE));

		switch (ptr->state) {
		case STATE::WAIT:
			if (!Packing_Recv(ptr->tcp_sock, ptr->buf)) {
				flag = true;
			}

			protocol = (PROTOCOL)GetProtocol(ptr->buf);
			switch (protocol) {
			case PROTOCOL::CALL:
				ptr->state = STATE::VIEW;
				break;

			case PROTOCOL::QUIT:
				ptr->state = STATE::EXIT;
				break;
			}
			break;

		case STATE::VIEW:
			size = Packing(ptr->buf, (int)PROTOCOL::VIEWROOM, roominfo);
			retval = send(ptr->tcp_sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send");
				flag = true;
			}
			printf("[TCP / %s : %d] 방 이름 정보 수신 완료\n", inet_ntoa(ptr->tcp_addr.sin_addr),
				ntohs(ptr->tcp_addr.sin_port));
			ptr->state = STATE::SELECT;
			break;

		case STATE::SELECT:
			if (!Packing_Recv(ptr->tcp_sock, ptr->buf)) {
				flag = true;
			}

			UnPacking(ptr->buf, num);
			ZeroMemory(&ip, sizeof(MSGSIZE));
			if (RoomSelect(num, ip)) {
				ptr->state = STATE::ROOMIP;
			}
			else {
				ptr->state = STATE::EXIT;
			}
			break;

		case STATE::ROOMIP:
			size = Packing(ptr->buf, (int)PROTOCOL::SEND_ROOMIP, ip, user_count[num]);
			retval = send(ptr->tcp_sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send");
				flag = true;
			}
			printf("[TCP / %s : %d] 방 주소 정보 송신 완료\n", inet_ntoa(ptr->tcp_addr.sin_addr),
				ntohs(ptr->tcp_addr.sin_port));
			
			ptr->state = STATE::IN_ROOM;
			break;

		case STATE::IN_ROOM:
			ZeroMemory(&buf, sizeof(BUFSIZE));
			ZeroMemory(&user, sizeof(User_Info));

			if (!Packing_Recv(ptr->tcp_sock, ptr->buf)) {
				flag = true;
			}

			retval = UnPacking(ptr->buf, user.name);
			user.name[retval] = '\0';
			sprintf_s(user.msg, "[ %s 님이 입장하셨습니다 ]", user.name);

			Add(num, user_count[num], user.name);
			memcpy(&user.room_user, &member_list[num], sizeof(user.room_user));

			EnterCriticalSection(&cs);
			user_count[num] += 1;
			user.room_count = user_count[num];
			LeaveCriticalSection(&cs);

			printf("%s User Join\n", user.name);

			udp_addr = UDP_Address_Setup(ip);
			memcpy(buf, &user, sizeof(User_Info));

			retval = sendto(udp_sock, buf, sizeof(User_Info), 0,
				(SOCKADDR*)&udp_addr, sizeof(udp_addr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto");
			}

			ptr->state = OUT_ROOM;
			break;

		case STATE::OUT_ROOM:
			ZeroMemory(&buf, sizeof(BUFSIZE));
			ZeroMemory(&user, sizeof(User_Info));

			if (!Packing_Recv(ptr->tcp_sock, ptr->buf)) {
				flag = true;
			}

			retval = UnPacking(ptr->buf, user.name);
			user.name[retval] = '\0';
			sprintf_s(user.msg, "[ %s 님이 퇴장하셨습니다 ]", user.name);

			Remove(num, user_count[num], user.name);
			memcpy(&user.room_user, &member_list[num], sizeof(user.room_user));

			EnterCriticalSection(&cs);
			user_count[num] -= 1;
			user.room_count = user_count[num];
			LeaveCriticalSection(&cs);

			printf("%s User Out\n", user.name);

			udp_addr = UDP_Address_Setup(ip);
			memcpy(buf, &user, sizeof(User_Info));

			retval = sendto(udp_sock, buf, sizeof(User_Info), 0,
				(SOCKADDR*)&udp_addr, sizeof(udp_addr));
			if (retval == SOCKET_ERROR) {
				err_display("sendto");
			}

			ptr->state = WAIT;
			break;

		case STATE::EXIT:
			flag = true;
			break;
		}
	}

	Remove_Client(ptr);
	return 0;
}