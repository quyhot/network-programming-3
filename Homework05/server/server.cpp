// WSAAsyncSelectServer.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Resource.h"
#include "winsock2.h"
#include "windows.h"
#include "stdio.h"
#include "conio.h"
#include "ws2tcpip.h"
#include "iostream"
#include "fstream"
#include "string"
#include "process.h"
#include "ctime"
#include <atlstr.h>

using namespace std;

#define WM_SOCKET WM_USER + 1
#define SERVER_PORT 6000
#define MAX_CLIENT 1024
#define BUFF_SIZE 2048
#define SERVER_ADDR "127.0.0.1"
#define USER_LEN 200
#define DELIMITER "\r\n"
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable:4996)

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
HWND				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	windowProc(HWND, UINT, WPARAM, LPARAM);

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable:4996)

SOCKET listenSock;
SOCKET acceptSock;
fstream accountFile;
ofstream logFile;
sockaddr_in clientAddr;
int clientLength = sizeof(clientAddr);
fd_set readfds, initfds;
int nEvents;

/*
struct client session in server, struct include:
@param sock - socket used to connect the client
@param clientIP - client ip
@param clientPort - client port
@param username - username that client used to login
@param isLogin - Login status
*/
struct session {
	SOCKET sock;
	char clientIP[INET_ADDRSTRLEN];
	int clientPort;
	char username[USER_LEN];
	bool isLogin;
};
session client[MAX_CLIENT];

// Send message
void sendMessage(char *buff, SOCKET &connectedSocket) {

	int ret = send(connectedSocket, buff, strlen(buff), 0);
	if (ret == SOCKET_ERROR) {
		printf("Error %d: Can't send data.\n", WSAGetLastError());
	}
}

// Write in file log
void writeInLogFile(string log) {
	logFile.open("log_20183816.txt", ios::out | ios::app);
	if (logFile.is_open()) {
		logFile << log + "\n";
		logFile.close();
	}
	else cout << "Unable to open file.";
}

// Login function handle login request from client
// @param data - message without protocol send by client
// @param clientSession - Pointer client session
// @param log - reference variable store the activity log 
void logIn(string data, session *clientSession, string &log) {
	char rs[BUFF_SIZE];
	memset(rs, 0, BUFF_SIZE);
	accountFile.open("account.txt", ios::in);
	string line;
	// Read file account.txt and check account status
	while (!accountFile.eof())
	{
		getline(accountFile, line);
		string username = line.substr(0, line.find(" "));
		if (username == data) {
			// check account status
			string status = line.substr(line.find(" ") + 1);
			if (status == "1") {
				strcat_s(rs, "407 User has been locked");
				log += "407";
			}
			else {
				strcat_s(rs, "200 Login successful");
				log += "200";
				clientSession->isLogin = true;
				strcpy(clientSession->username, username.c_str());
			}
			break;
		}
	}
	if (strlen(rs) == 0) {
		strcat_s(rs, "406 User does not exist!");
		log += "406";
	}
	sendMessage(rs, clientSession->sock);
	// Write in log file
	writeInLogFile(log);
	// close file "account.txt"
	accountFile.close();
}

// postMessage function handle post message request
// @param data - message without protocol send by client
// @param clientSession - Pointer client session
// @param log - reference variable store the activity log 
void postMessage(string data, session *clientSession, string &log) {
	char rs[BUFF_SIZE];
	memset(rs, 0, BUFF_SIZE);

	strcat_s(rs, "200 Post sucessful!");
	log += "200";
	// Send Message to Client
	sendMessage(rs, clientSession->sock);
	// Write in log file
	writeInLogFile(log);
}

// Handle Log Out Request
// @param clientSession - Pointer client session
// @param log - reference variable store the activity log 
void logOut(session *clientSession, string &log) {
	char rs[BUFF_SIZE];
	memset(rs, 0, BUFF_SIZE);
	// Handle critical resource

	strcat_s(rs, "200 Logout sucessfull!");
	log += "200";
	clientSession->isLogin = false;
	memset(clientSession->username, 0, USER_LEN);

	// Send message
	sendMessage(rs, clientSession->sock);
	// Write in log file
	writeInLogFile(log);
}

// Split protocol and message from client, handle protocol
// @param buff - Pointer to input string
// @param clientSession - Pointer client session
// @param log - reference variable store the activity log 
void handleProtocol(char *buff, session *clientSession, string &log) {

	string str(buff);
	// Write message to log variable
	log += str + " $ ";
	string key = str.substr(0, 4);
	string data;
	if (str.length() > 4) {
		data = str.substr(5);
	}
	if (key == "USER") {
		if (clientSession->isLogin) {
			// check login
			log += "401";
			buff = "401 you are logged in, Please log out first!";
			sendMessage(buff, clientSession->sock);
			writeInLogFile(log);
		}
		else {
			logIn(data, clientSession, log);
		}
	}
	else if (key == "POST") {
		if (!clientSession->isLogin) {
			// check login
			log += "402";
			buff = "402 You are not log in!";
			sendMessage(buff, clientSession->sock);
			writeInLogFile(log);
		}
		else {
			postMessage(data, clientSession, log);
		}

	}
	else if (key == "QUIT") {
		if (!clientSession->isLogin) {
			// check login
			log += "402";
			buff = "402 You are not log in!";
			sendMessage(buff, clientSession->sock);
			writeInLogFile(log);
		}
		else {
			logOut(clientSession, log);
		}
	}
	else {
		log += "403";
		buff = "403 Wrong protocol!";
		// Send message
		sendMessage(buff, clientSession->sock);
		// Write in log file
		writeInLogFile(log);
	}
}

// Return current time when user send message to server
void returnCurrentTime(string &log) {
	log += "[";
	time_t current = time(0); // current time
	tm *ltm = localtime(&current);
	log += to_string(ltm->tm_mday) + "/"; // day
	log += to_string(ltm->tm_mon + 1) + "/"; // month
	log += to_string(ltm->tm_year + 1900) + " "; // year
	log += to_string(ltm->tm_hour) + ":"; // hour
	log += to_string(ltm->tm_min) + ":"; // minutes
	log += to_string(ltm->tm_sec) + "]" + " $ "; // seconds
}

// receive message from client
// @param buff - Pointer to input string
// @param clientSession - Pointer client session
int recvMessage(char *buff, session *clientSession) {
	// Handle byte stream
	string buffRecv;
	memset(buff, 0, BUFF_SIZE);
	while (1) {
		int ret = recv(clientSession->sock, buff, BUFF_SIZE, 0);
		if (ret <= 0) {
			return ret;
		}
		else {
			buff[ret] = 0;
			buffRecv += buff;
			if (strstr(buff, DELIMITER)) {
				buffRecv = buffRecv.substr(0, buffRecv.find(DELIMITER));
				strcpy(buff, buffRecv.c_str());
				return ret;
			}
		}
	}
}

// Close socket in client's socket array
// @param i - client's position in client array
void closeSock(int i) {
	closesocket(client[i].sock);
	client[i].isLogin = false;
	memset(client[i].username, 0, USER_LEN);
	memset(client[i].clientIP, 0, BUFF_SIZE);
	client[i].clientPort = 0;
	client[i].sock = 0;
}

// Communicate with client
// @param i - client's position in client array
void communicateClient(int i) {
	string log;
	char buff[BUFF_SIZE];
	// receive and handle message
	int ret = recvMessage(buff, client + i);
	if (ret > 0) {
		// write clientIp and clientPort to log variable 
		log = client[i].clientIP;
		log += ":" + to_string(client[i].clientPort);
		// write current time to log variable 
		returnCurrentTime(log);
		// handle message
		handleProtocol(buff, client + i, log);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

	MSG msg;
	HWND serverWindow;

	//Registering the Window Class
	MyRegisterClass(hInstance);

	//Create the window
	if ((serverWindow = InitInstance(hInstance, nCmdShow)) == NULL)
		return FALSE;

	//Initiate WinSock
	WSADATA wsaData;
	WORD wVersion = MAKEWORD(2, 2);
	if (WSAStartup(wVersion, &wsaData)) {
		MessageBox(serverWindow, L"Winsock 2.2 is not supported.", L"Error!", MB_OK);
		return 0;
	}

	//Construct socket	
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//requests Windows message-based notification of network events for listenSock
	WSAAsyncSelect(listenSock, serverWindow, WM_SOCKET, FD_ACCEPT | FD_CLOSE | FD_READ);

	//Bind address to socket
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);

	if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr)))
	{
		MessageBox(serverWindow, L"Cannot associate a local address with server socket.", L"Error!", MB_OK);
	}

	//Listen request from client
	if (listen(listenSock, MAX_CLIENT)) {
		MessageBox(serverWindow, L"Cannot place server socket in state LISTEN.", L"Error!", MB_OK);
		return 0;
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = windowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SERVER));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"WindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;
	memset(client, 0, MAX_CLIENT);
	hWnd = CreateWindow(L"WindowClass", L"Server window", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
		return FALSE;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_SOCKET	- process the events on the sockets	
//  WM_DESTROY	- post a quit message and return
//
//

LRESULT CALLBACK windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int i;
	switch (message) {
	case WM_SOCKET:
	{
		if (WSAGETSELECTERROR(lParam)) {
			for (i = 0; i < MAX_CLIENT; i++)
				if (client[i].sock == (SOCKET)wParam) {
					closeSock(i);
					continue;
				}
		}

		switch (WSAGETSELECTEVENT(lParam)) {
		case FD_ACCEPT:
		{
			acceptSock = accept((SOCKET)wParam, (sockaddr *)&clientAddr, &clientLength);
			for (i = 0; i < MAX_CLIENT; i++)
				if (client[i].sock == 0) {
					client[i].sock = acceptSock;
					inet_ntop(AF_INET, &clientAddr.sin_addr, client[i].clientIP, sizeof(client[i].clientIP));
					client[i].clientPort = ntohs(clientAddr.sin_port);
					//requests Windows message-based notification of network events for listenSock
					WSAAsyncSelect(client[i].sock, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
					break;
				}
			if (i == MAX_CLIENT)
				MessageBox(hWnd, L"Too many clients!", L"Notice", MB_OK);
		}
		break;

		case FD_READ:
		{
			for (i = 0; i < MAX_CLIENT; i++)
				if (client[i].sock == (SOCKET)wParam)
					break;
			communicateClient(i);
		}
		break;

		case FD_CLOSE:
		{
			for (i = 0; i < MAX_CLIENT; i++)
				if (client[i].sock == (SOCKET)wParam) {
					closeSock(i);
					break;
				}
		}
		break;
		}
	}
	break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
	}
	break;

	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
		shutdown(listenSock, SD_BOTH);
		closesocket(listenSock);
		WSACleanup();
		return 0;
	}
	break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}