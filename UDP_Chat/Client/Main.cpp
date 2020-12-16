#include "MyFunction.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HINSTANCE g_hInst;
HWND hWndMain;
LPCTSTR lpszClass = TEXT("Client");

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance
	, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASS WndClass;
	g_hInst = hInstance;

	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hInstance = hInstance;
	WndClass.lpfnWndProc = WndProc;
	WndClass.lpszClassName = lpszClass;
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
		0, 0, 800, 600,
		NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);

	while (GetMessage(&Message, NULL, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return (int)Message.wParam;
}

User_Info user;
ROOMINFO room;
DISPLAYSTATE display;
Client_Info* client;

HANDLE hThread;

POINT b_pos;
POINT m_pos;

int len = 0;
bool memberlist;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam) 
{
	HDC hdc;
	HDC backDC, DBDC;
	HBITMAP DB, oldDB;
	PAINTSTRUCT ps;
	static RECT rt;

	WSADATA wsa;

	switch (iMessage) {
	case WM_CREATE:
		SetTimer(hWnd, 0, 100, nullptr);

		ZeroMemory(&user, sizeof(User_Info));
		ZeroMemory(&client, sizeof(client));

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != NULL) err_quit("WSAStartup()");
		client = new Client_Info();
		// UDP 소켓 초기화
		UDP_SocketInit(client->udp_sock, client->udp_addr, 2);
		display = LOGIN;

		hThread = CreateThread(nullptr, 0, RecvThread, (LPVOID)client, 0, nullptr);
		if (hThread == NULL) {
			MessageBox(nullptr, "스레드 생성 실패", "Error", MB_OK);
			ExitProcess(0);
		}
		CloseHandle(hThread);
		break;

	case WM_CHAR:
		if (wParam != VK_RETURN && wParam != VK_ESCAPE)
		{
			switch (display) {
			case LOGIN:
				InputChar(wParam, user.name, &len, 20);
				break;

			case CHAT:
				InputChar(wParam, user.chat, &len, 50);
				break;
			}
		}
		break;

	case WM_KEYDOWN:
		switch (wParam) {
		case VK_RETURN:
			switch (display) {
			case LOGIN:
				if (len > 1) {
					// TCP 소켓 초기화 및 커넥트
					TCP_SocketInit(client->tcp_sock, client->tcp_addr);
					TCP_Connect(client);

					client->state = false;
					if (!RoomView(client, room)) {
						closesocket(client->tcp_sock);
					}
					else {
						display = ROOM;
					}
				}
				else {
					MessageBox(nullptr, "3 글자 이상 입력하세요", "Error", MB_OK);
				}
				break;

			case CHAT:
				if (user.chat[0] != NULL) {
					if (!SendToMessage(client, user)) {
						ZeroMemory(&user.chat, sizeof(user.chat));
						ZeroMemory(&client->TextArray, sizeof(client->TextArray));
						client->current = 0;
						client->state = false;
						memberlist = false;

						Multicast_Group_Drop(client->udp_sock, client->mreq);
						ZeroMemory(&client->mreq, sizeof(ip_mreq));

						closesocket(client->tcp_sock);
						display = LOGIN;
						len = strlen(user.name);
					}
				}
				break;
			}
			break;

		case VK_ESCAPE:
			switch (display) {
			case LOGIN:
				KillTimer(hWnd, 0);
				CleanSocket(client);
				delete client;

				PostQuitMessage(0);
				break;
				
			case ROOM:
				RoomSelect(client, user, NULL);
				closesocket(client->tcp_sock);
				display = LOGIN;
				break;

			case CHAT:
				if (memberlist) {
					memberlist = false;
				}
				else {
					ZeroMemory(&user.chat, sizeof(user.chat));
					ZeroMemory(&client->TextArray, sizeof(client->TextArray));
					client->current = 0;
					client->state = false;
					memberlist = false;

					if (!OutMessage(client, user)) {
						closesocket(client->tcp_sock);
						display = LOGIN;
					}
					else {
						Multicast_Group_Drop(client->udp_sock, client->mreq);
						ZeroMemory(&client->mreq, sizeof(ip_mreq));

						if (!RoomView(client, room)) {
							closesocket(client->tcp_sock);
							display = LOGIN;
						}
						else {
							display = ROOM;
						}
					}
					len = strlen(user.name);
				}
				break;
			}
			break;
		}
		break;

	case WM_LBUTTONDOWN:
		GetCursorPos(&m_pos);
		ScreenToClient(hWnd, &m_pos);

		switch (display) {
		case LOGIN:
			// Login Input
			if (CollisionEnterPoint(400, 340, m_pos.x, m_pos.y, 50, 20)) {
				if (len > 1) {
					// TCP 소켓 초기화 및 커넥트
					TCP_SocketInit(client->tcp_sock, client->tcp_addr);
					TCP_Connect(client);

					client->state = false;
					if (!RoomView(client, room)) {
						closesocket(client->tcp_sock);
					}
					else {
						display = ROOM;
					}
				}
				else {
					MessageBox(nullptr, "3 글자 이상 입력하세요", "Error", MB_OK);
				}
			}
			break;

		case ROOM:
			// Room Select
			for (int i = 0; i < room.size; i++) {
				if (CollisionEnterPoint(400, (70 + (70 * i)), m_pos.x, m_pos.y, 200, 30)) {
					if (!RoomSelect(client, user, i + 1)) {
						closesocket(client->tcp_sock);
						display = LOGIN;
					}
					else {
						user.room_num = i;
						display = CHAT;

						if (!Multicast_Group_Add(client->udp_sock, client->mreq, user.room_ip)) {
							closesocket(client->tcp_sock);
							display = LOGIN;
						}
						else {
							client->state = true;
							UDP_Address_Setup(client->udp_addr2, user.room_ip);

							if (!JoinMessage(client, user)) {
								Multicast_Group_Drop(client->tcp_sock, client->mreq);
								ZeroMemory(&client->mreq, sizeof(ip_mreq));
								closesocket(client->tcp_sock);
								display = LOGIN;
							}
						}
					}
					break;
				}
			}

			// Back
			if (CollisionEnterPoint(400, 450, m_pos.x, m_pos.y, 50, 20)) {
				RoomSelect(client, user, NULL);
				closesocket(client->tcp_sock);
				display = LOGIN;
			}
			break;

		case CHAT:
			// Chat Input
			if (CollisionEnterPoint(600, 500, m_pos.x, m_pos.y, 40, 20) && user.chat[0] != NULL) {
				if (!SendToMessage(client, user)) {
					ZeroMemory(&user.chat, sizeof(user.chat));
					ZeroMemory(&client->TextArray, sizeof(client->TextArray));
					client->current = 0;
					client->state = false;
					memberlist = false;

					Multicast_Group_Drop(client->udp_sock, client->mreq);
					ZeroMemory(&client->mreq, sizeof(ip_mreq));

					closesocket(client->tcp_sock);
					display = LOGIN;
					len = strlen(user.name);
				}
			}

			// Member list
			if (CollisionEnterPoint(700, 25, m_pos.x, m_pos.y, 40, 20)) {
				if (!memberlist) memberlist = true;
				else memberlist = false;
			}

			// Back
			if (CollisionEnterPoint(700, 500, m_pos.x, m_pos.y, 40, 20)) {
				ZeroMemory(&user.chat, sizeof(user.chat));
				ZeroMemory(&client->TextArray, sizeof(client->TextArray));
				client->current = 0;
				client->state = false;
				memberlist = false;

				if (!OutMessage(client, user)) {
					closesocket(client->tcp_sock);
					display = LOGIN;
				}
				else {
					Multicast_Group_Drop(client->udp_sock, client->mreq);
					ZeroMemory(&client->mreq, sizeof(ip_mreq));

					if (!RoomView(client, room)) {
						closesocket(client->tcp_sock);
						display = LOGIN;
					}
					else {
						display = ROOM;
					}
				}
				len = strlen(user.name);
			}
			break;
		}
		break;

	case WM_PAINT:
		GetClientRect(hWnd, &rt);
		hdc = BeginPaint(hWnd, &ps);
		DBDC = CreateCompatibleDC(hdc);
		DB = CreateCompatibleBitmap(hdc, rt.right, rt.bottom);
		oldDB = (HBITMAP)SelectObject(DBDC, DB);
		PatBlt(DBDC, 0, 0, rt.right, rt.bottom, WHITENESS);
		backDC = hdc;
		hdc = DBDC;
		DBDC = backDC;
		// --------------------------------------------------------------------------

		switch (display) {
		case LOGIN:
			LoginDisplay(hWnd, hdc, user);
			break;

		case ROOM:
			RoomSelectDisPlay(hWnd, hdc, user, room);
			break;

		case CHAT:
			ChattingDisPlay(hWnd, hdc, client, user, room);
			break;
		}

		if (memberlist) {
			MemberDIsPlay(hWnd, hdc, client);
		}

		// --------------------------------------------------------------------------
		backDC = hdc;
		hdc = DBDC;
		DBDC = backDC;
		BitBlt(hdc, 0, 0, rt.right, rt.bottom, DBDC, 0, 0, SRCCOPY);
		SelectObject(DBDC, oldDB);
		DeleteObject(DB);
		DeleteDC(DBDC);
		DeleteDC(backDC);

		EndPaint(hWnd, &ps);
		break;

	case WM_TIMER:
		InvalidateRect(hWnd, &rt, false);
		break;

	case WM_DESTROY:
		client->state = false;
		if (display == CHAT)
			OutMessage(client, user);
		KillTimer(hWnd, 0);		
		CleanSocket(client);
		delete client;

		PostQuitMessage(0);
		return 0;
	}
	return(DefWindowProc(hWnd, iMessage, wParam, lParam)); 
}