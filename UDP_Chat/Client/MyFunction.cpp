#include "MyFunction.h"

void Add(char Arr[][MSGSIZE], int* count, ElementType data) {
	if (*count > MAX_SIZE - 1) {
		Sort(Arr, *count);
		strcpy(Arr[*count - 1], data);
	}
	else {
		strcpy(Arr[*count], data);
		*count += 1;
	}
}

void Remove(char Arr[][MSGSIZE], int* count, ElementType data) {
	char temp[128];
	for (int i = 0; i < *count; i++) {
		if (!strcmp(Arr[i], data)) {
			for (int j = i; j < *count - 1; j++) {
				strcpy(temp, Arr[j]);
				strcpy(Arr[j], Arr[j + 1]);
				strcpy(Arr[j + 1], temp);
			}
			break;
		}
	}

	ZeroMemory(&Arr[*count], sizeof(MSGSIZE));
	*count -= 1;
}

void Sort(char Arr[][MSGSIZE], int count) {
	char temp[128];
	for (int i = 0; i < count - 1; i++) {
		strcpy(temp, Arr[i]);
		strcpy(Arr[i], Arr[i + 1]);
		strcpy(Arr[i + 1], temp);
	}
}

void InputChar(WPARAM wParam, char* text, int* len, int size) {
	*len = strlen(text);
	if (wParam == VK_BACK && *len == NULL) {
		return;
	}
	if (wParam == VK_BACK && *len != NULL) {
		text[*len - 1] = NULL;
	}
	if (*len <= size) {
		text[*len] = (char)wParam;
		text[*len + 1] = NULL;
	}
}

bool CollisionEnterPoint(int x, int y, int mx, int my, int x_size, int y_size) {
	if ((x + x_size >= mx && mx >= x - x_size) && (y + y_size >= my && my >= y - y_size)) {
		return true;
	}
	else {
		return false;
	}
}

bool RoomView(Client_Info* ptr, ROOMINFO& room) {
	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));

	int size = Packing(ptr->buf, PROTOCOL::CALL);
	int retval = send(ptr->tcp_sock, ptr->buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("send");
		return false;
	}

	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));
	if (!packing_recvn(ptr->tcp_sock, ptr->buf)) {
		return false;
	}
	UnPacking(ptr->buf, room);
	return true;
}

bool RoomSelect(Client_Info* ptr, User_Info& user, int num) {
	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));

	int size = Packing(ptr->buf, PROTOCOL::SELECT, num);
	int retval = send(ptr->tcp_sock, ptr->buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("send");
		return false;
	}
	if (num == NULL) {
		return false;
	}

	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));
	if (!packing_recvn(ptr->tcp_sock, ptr->buf)) {
		return false;
	}

	UnPacking(ptr->buf, user.room_ip, ptr->room_count);
	return true;
}

bool JoinMessage(Client_Info* ptr, User_Info user) {
	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));
	
	int size = Packing(ptr->buf, PROTOCOL::IN_ROOM, user.name);
	int retval = send(ptr->tcp_sock, ptr->buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("send");
		return false;
	}
	return true;
}

bool OutMessage(Client_Info* ptr, User_Info user) {
	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));

	int size = Packing(ptr->buf, PROTOCOL::OUT_ROOM, user.name);
	int retval = send(ptr->tcp_sock, ptr->buf, size, 0);
	if (retval == SOCKET_ERROR) {
		err_display("send");
		return false;
	}
	return true;
}

bool SendToMessage(Client_Info* ptr, User_Info& user) {
	int retval;

	ZeroMemory(&ptr->buf, sizeof(BUFSIZE));
	memcpy(ptr->buf, &user, sizeof(User_Info));

	retval = sendto(ptr->udp_sock, ptr->buf, sizeof(ptr->buf), 0, 
		(SOCKADDR*)&ptr->udp_addr2, sizeof(ptr->udp_addr2));
	if (retval == SOCKET_ERROR) {
		err_display("send");
		return false;
	}

	ZeroMemory(&user.chat, sizeof(user.chat));
	return true;
}

DWORD WINAPI RecvThread(LPVOID arg) {
	int retval;
	Client_Info* sock = (Client_Info*)arg;

	User_Info user;
	char msg[MSGSIZE];
	int addrlen;

	while (1) {
		ZeroMemory(&sock->buf, sizeof(sock->buf));
		addrlen = sizeof(sock->peeraddr);
		retval = recvfrom(sock->udp_sock, sock->buf, sizeof(sock->buf), 0,
			(SOCKADDR*)&sock->peeraddr, &addrlen);
		if (retval == SOCKET_ERROR) {
			err_display("recv");
			break;
		}

		memcpy(&user, sock->buf, sizeof(User_Info));
		if (user.chat[0] != NULL) {
			sprintf_s(msg, "[%d] %s ดิ : %s", ntohs(sock->peeraddr.sin_port), user.name, user.chat);
			Add(sock->TextArray, &sock->current, msg);
		}
		else {
			sock->room_count = user.room_count;
			memcpy(sock->room_user, &user.room_user, sizeof(user.room_user));
			Add(sock->TextArray, &sock->current, user.msg);
		}
	}
	return 0;
}

void LoginDisplay(HWND hWnd, HDC hdc, User_Info user) {
	char Text[128];

	Rectangle(hdc, 400 - 150, 300 - 70, 400 + 150, 300 + 70);
	Rectangle(hdc, 400 - 120, 280 - 15, 400 + 120, 280 + 15);
	TextOut(hdc, 290, 270, user.name, strlen(user.name));
	strcpy(Text, "Name");
	TextOut(hdc, 380, 240, Text, strlen(Text));

	Rectangle(hdc, 400 - 50, 340 - 20, 400 + 50, 340 + 20);
	strcpy(Text, "Enter or Click");
	TextOut(hdc, 357, 330, Text, strlen(Text));
}

void RoomSelectDisPlay(HWND hWnd, HDC hdc, User_Info user, ROOMINFO room) {
	char Text[128];

	for (int i = 0; i < room.size; i++) {
		Rectangle(hdc, 400 - 200, (70 + (70 * i)) - 30, 400 + 200, (70 + (70 * i)) + 30);
		TextOut(hdc, 375, (60 + (70 * i)), room.name[i], strlen(room.name[i]));
	}

	// Back
	Rectangle(hdc, 400 - 50, 450 - 20, 400 + 50, 450 + 20);
	strcpy(Text, "Back");
	TextOut(hdc, 385, 440, Text, strlen(Text));
}

void ChattingDisPlay(HWND hWnd, HDC hdc, Client_Info* ptr, User_Info user, ROOMINFO room) {
	char Text[128];

	// Info
	Rectangle(hdc, 400 - 100, 25 - 20, 400 + 100, 25 + 20);
	TextOut(hdc, 370, 17, room.name[user.room_num], strlen(room.name[user.room_num]));

	// input
	Rectangle(hdc, 290 - 250, 500 - 20, 290 + 250, 500 + 20);
	TextOut(hdc, 60, 490, user.chat, strlen(user.chat));

	// output
	Rectangle(hdc, 290 - 250, 250 - 200, 290 + 450, 250 + 200);
	for (int i = 0; i < ptr->current; i++) {
		TextOut(hdc, 60, (60 + (20 * i)), ptr->TextArray[i], strlen(ptr->TextArray[i]));
	}

	// Join User Count
	Rectangle(hdc, 80 - 40, 25 - 20, 80 + 40, 25 + 20);
	sprintf_s(Text, "Join : %d", ptr->room_count);
	TextOut(hdc, 55, 15, Text, strlen(Text));

	// Join User List
	Rectangle(hdc, 700 - 40, 25 - 20, 700 + 40, 25 + 20);
	strcpy(Text, "Member");
	TextOut(hdc, 675, 15, Text, strlen(Text));

	// enter
	Rectangle(hdc, 600 - 40, 500 - 20, 600 + 40, 500 + 20);
	strcpy(Text, "Enter");
	TextOut(hdc, 585, 490, Text, strlen(Text));

	// exit
	Rectangle(hdc, 700 - 40, 500 - 20, 700 + 40, 500 + 20);
	strcpy(Text, "Back");
	TextOut(hdc, 685, 490, Text, strlen(Text));
}

void MemberDIsPlay(HWND hWnd, HDC hdc, Client_Info* ptr) {
	Rectangle(hdc, 640 - 100, 250 - 200, 640 + 100, 250 + 200);
	for (int i = 0; i < ptr->room_count; i++) {
		TextOut(hdc, 548, (60 + (20 * i)), ptr->room_user[i], strlen(ptr->room_user[i]));
	}
}