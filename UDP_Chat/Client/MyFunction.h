#pragma once

#include "Socket.h"
#include <Windows.h>
#include <math.h>

void Add(char Arr[][MSGSIZE], int* count, ElementType data);
void Remove(char Arr[][MSGSIZE], int* count, ElementType data);
void Sort(char Arr[][MSGSIZE], int count);

void InputChar(WPARAM wParam, char* text, int* len, int size);
bool CollisionEnterPoint(int x, int y, int mx, int my, int x_size, int y_size);

bool RoomView(Client_Info* ptr, ROOMINFO& room);
bool RoomSelect(Client_Info* ptr, User_Info& user, int num);

bool JoinMessage(Client_Info* ptr, User_Info user);
bool OutMessage(Client_Info* ptr, User_Info user);
bool SendToMessage(Client_Info* ptr, User_Info& user);

DWORD WINAPI RecvThread(LPVOID arg);

void LoginDisplay(HWND hWnd, HDC hdc, User_Info user);
void RoomSelectDisPlay(HWND hWnd, HDC hdc, User_Info user, ROOMINFO room);
void ChattingDisPlay(HWND hWnd, HDC hdc, Client_Info* ptr, User_Info user, ROOMINFO room);
void MemberDIsPlay(HWND hWnd, HDC hdc, Client_Info* ptr);