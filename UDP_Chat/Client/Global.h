#pragma once

#define IP "127.0.0.1"
#define PORT 9050
#define BUFSIZE 4096

#define MSGSIZE 128
#define ROOM_SIZE 10
#define MAX_SIZE 20

struct Client_Info;

enum DISPLAYSTATE { LOGIN, ROOM, CHAT };

enum PROTOCOL {
	CALL,
	SELECT,
	IN_ROOM,
	OUT_ROOM,
};

struct Client_Info {
	SOCKET tcp_sock;
	SOCKADDR_IN tcp_addr;

	SOCKET udp_sock;
	SOCKADDR_IN udp_addr;
	SOCKADDR_IN udp_addr2;

	SOCKADDR_IN peeraddr;
	struct ip_mreq mreq;

	char buf[BUFSIZE];

	int current;
	char TextArray[MAX_SIZE][MSGSIZE];

	char room_user[MAX_SIZE][MSGSIZE];
	int room_count;

	bool state;

	Client_Info() {
		ZeroMemory(&peeraddr, sizeof(SOCKADDR_IN));
		ZeroMemory(&mreq, sizeof(ip_mreq));
		ZeroMemory(&buf, sizeof(BUFSIZE));
		ZeroMemory(&TextArray, sizeof(TextArray));
		ZeroMemory(&room_user, sizeof(room_user));

		current = 0;
		room_count = 0;

		state = false;
	}
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

typedef char* ElementType;