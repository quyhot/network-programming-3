// SingleIOCPServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <conio.h>
#include <WS2tcpip.h>
#include <iostream>
#include "ctime"
#include "string"
#include "fstream"

using namespace std;

#define PORT 6000
#define DATA_BUFSIZE 8192
#define RECEIVE 0
#define SEND 1
#define SERVER_ADDR "127.0.0.1"
#define USER_LEN 200
#define DELIMITER "\r\n"

#pragma comment(lib, "Ws2_32.lib")

// Structure definition
typedef struct {
	WSAOVERLAPPED overlapped;
	WSABUF dataBuff;
	CHAR buffer[DATA_BUFSIZE];
	int bufLen;
	int recvBytes;
	int sentBytes;
	int operation;
	char clientIP[INET_ADDRSTRLEN];
	int clientPort;
	char username[USER_LEN];
	bool isLogin;
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;

typedef struct {
	SOCKET socket;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

CRITICAL_SECTION criticalSection;
int threadId = 0;
ofstream logFile;
fstream accountFile;

unsigned __stdcall serverWorkerThread(LPVOID CompletionPortID);
void communicateClient(LPPER_IO_OPERATION_DATA perIoData);
void returnCurrentTime(string &log);
void handleProtocol(LPPER_IO_OPERATION_DATA perIoData, string &log);
void writeInLogFile(string log);
void logIn(LPPER_IO_OPERATION_DATA perIoData, string &log, string data);
void logOut(LPPER_IO_OPERATION_DATA perIoData, string &log);
void postMessage(LPPER_IO_OPERATION_DATA perIoData, string &log, string data);
void sendMessage(char *buff, SOCKET &connectedSocket);

int main(int argc, char *argv[]) 
{
	SOCKADDR_IN serverAddr;
	SOCKET listenSock, acceptSock;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD transferredBytes;
	DWORD flags;
	WSADATA wsaData;

	if (WSAStartup((2, 2), &wsaData) != 0) {
		printf("WSAStartup() failed with error %d\n", GetLastError());
		return 1;
	}

	// Step 1: Setup an I/O completion port
	if ((completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
		printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
		return 1;
	}

	// Step 2: Determine how many processors are on the system
	GetSystemInfo(&systemInfo);

	// Step 3: Create worker threads based on the number of processors available on the
	// system. Create two worker threads for each processor	
	for (int i = 0; i < (int)systemInfo.dwNumberOfProcessors * 2; i++) {
		// Create a server worker thread and pass the completion port to the thread
		if (_beginthreadex(0, 0, serverWorkerThread, (void*)completionPort, 0, 0) == 0) {
			printf("Create thread failed with error %d\n", GetLastError());
			return 1;
		}
	}

	// Step 4: Create a listening socket
	if ((listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
		printf("WSASocket() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_ADDR, &serverAddr.sin_addr);
	if (bind(listenSock, (PSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		printf("bind() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	// Prepare socket for listening
	if (listen(listenSock, 20) == SOCKET_ERROR) {
		printf("listen() failed with error %d\n", WSAGetLastError());
		return 1;
	}

	printf("Server Started!");

	InitializeCriticalSection(&criticalSection);
	while (1) {
		sockaddr_in clientAddr;
		int clientLength = sizeof(clientAddr);

		// Step 5: Accept connections
		if ((acceptSock = WSAAccept(listenSock, (sockaddr *)&clientAddr, &clientLength, NULL, 0)) == SOCKET_ERROR) {
			printf("WSAAccept() failed with error %d\n", WSAGetLastError());
			return 1;
		}

		// Step 6: Create a socket information structure to associate with the socket
		if ((perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA))) == NULL) {
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			return 1;
		}

		// Step 7: Associate the accepted socket with the original completion port
		printf("Socket number %d got connected...\n", acceptSock);
		perHandleData->socket = acceptSock;
		if (CreateIoCompletionPort((HANDLE)acceptSock, completionPort, (DWORD)perHandleData, 0) == NULL) {
			printf("CreateIoCompletionPort() failed with error %d\n", GetLastError());
			return 1;
		}

		// Step 8: Create per I/O socket information structure to associate with the WSARecv call
		if ((perIoData = (LPPER_IO_OPERATION_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL) {
			printf("GlobalAlloc() failed with error %d\n", GetLastError());
			return 1;
		}

		ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
		perIoData->sentBytes = 0;
		perIoData->recvBytes = 0;
		perIoData->dataBuff.len = DATA_BUFSIZE;
		perIoData->dataBuff.buf = perIoData->buffer;
		perIoData->operation = RECEIVE;
		flags = 0;
		inet_ntop(AF_INET, &clientAddr.sin_addr, perIoData->clientIP, sizeof(perIoData->clientIP));
		perIoData->clientPort = ntohs(clientAddr.sin_port);
		perIoData->isLogin = false;

		if (WSARecv(acceptSock, &(perIoData->dataBuff), 1, &transferredBytes, &flags, &(perIoData->overlapped), NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != ERROR_IO_PENDING) {
				printf("WSARecv() failed with error %d\n", WSAGetLastError());
				return 1;
			}
		}
	}
	DeleteCriticalSection(&criticalSection);
	_getch();
	return 0;
}

unsigned __stdcall serverWorkerThread(LPVOID completionPortID)
{
	HANDLE completionPort = (HANDLE)completionPortID;
	DWORD transferredBytes;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_OPERATION_DATA perIoData;
	DWORD flags;

	char queue[DATA_BUFSIZE];

	while (TRUE) {
		if (GetQueuedCompletionStatus(completionPort, &transferredBytes,
			(LPDWORD)&perHandleData, (LPOVERLAPPED *)&perIoData, INFINITE) == 0) {
			printf("GetQueuedCompletionStatus() failed with error %d\n", GetLastError());
			return 0;
		}
		// Check to see if an error has occurred on the socket and if so
		// then close the socket and cleanup the SOCKET_INFORMATION structure
		// associated with the socket
		if (transferredBytes == 0) {
			printf("Closing socket %d\n", perHandleData->socket);
			if (closesocket(perHandleData->socket) == SOCKET_ERROR) {
				printf("closesocket() failed with error %d\n", WSAGetLastError());
				return 0;
			}
			GlobalFree(perHandleData);
			GlobalFree(perIoData);
			continue;
		}
		// Check to see if the operation field equals RECEIVE. If this is so, then
		// this means a WSARecv call just completed so update the recvBytes field
		// with the transferredBytes value from the completed WSARecv() call
		if (perIoData->operation == RECEIVE) {
			char rs[DATA_BUFSIZE];
			ZeroMemory(&rs, sizeof(DATA_BUFSIZE));
			strcpy(queue, perIoData->buffer);
			// Handle byte stream
			while (strstr(queue, DELIMITER) != NULL)
			{
				string strQueue = string(queue);
				string data = strQueue.substr(0, strQueue.find(DELIMITER));
				strcpy(perIoData->buffer, data.c_str());
				communicateClient(perIoData);
				strcpy(queue, strstr(queue, DELIMITER) + strlen(DELIMITER));
				if (strlen(queue) != 0) {
					sendMessage(perIoData->buffer, perHandleData->socket);
				}
				else {
					strcat(rs, perIoData->buffer);
				}
			}
			strcpy(perIoData->buffer, rs);
			perIoData->recvBytes = strlen(perIoData->buffer);
			perIoData->sentBytes = 0;
			perIoData->operation = SEND;
		}
		else if (perIoData->operation == SEND) {
			perIoData->sentBytes += transferredBytes;
		}

		if (perIoData->recvBytes > perIoData->sentBytes) {
			// Post another WSASend() request.
			// Since WSASend() is not guaranteed to send all of the bytes requested,
			// continue posting WSASend() calls until all received bytes are sent.
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.buf = perIoData->buffer + perIoData->sentBytes;
			perIoData->dataBuff.len = perIoData->recvBytes - perIoData->sentBytes;
			perIoData->operation = SEND;
			if (WSASend(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&transferredBytes,
				0,
				&(perIoData->overlapped),
				NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					printf("WSASend() failed with error %d\n", WSAGetLastError());
					return 0;
				}
			}
		}
		else {
			// No more bytes to send post another WSARecv() request
			perIoData->recvBytes = 0;
			perIoData->operation = RECEIVE;
			flags = 0;
			ZeroMemory(&(perIoData->overlapped), sizeof(OVERLAPPED));
			perIoData->dataBuff.len = DATA_BUFSIZE;
			perIoData->dataBuff.buf = perIoData->buffer;
			ZeroMemory(&(perIoData->buffer), sizeof(OVERLAPPED));
			if (WSARecv(perHandleData->socket,
				&(perIoData->dataBuff),
				1,
				&transferredBytes,
				&flags,
				&(perIoData->overlapped), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() != ERROR_IO_PENDING) {
					printf("WSARecv() failed with error %d\n", WSAGetLastError());
					return 0;
				}
			}
		}
	}
}

// Communicate with client
// @param perIoData - Pointer input data and info client
void communicateClient(LPPER_IO_OPERATION_DATA perIoData) {
	string log;
	// write clientIp and clientPort to log variable 
	log = perIoData->clientIP;
	log += ":" + to_string(perIoData->clientPort); 
	// write current time to log variable 
	returnCurrentTime(log);
	// handle message
	handleProtocol(perIoData, log);
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

// Split protocol and message from client, handle protocol
// @param perIoData - Pointer input data and info client
// @param log - reference variable store the activity log 
void handleProtocol(LPPER_IO_OPERATION_DATA perIoData, string &log) {

	string str(perIoData->buffer);
	// Write message to log variable
	log += str + " $ ";
	string key = str.substr(0, 4);
	string data;
	if (str.length() > 4) {
		data = str.substr(5);
	}
	if (key == "USER") {
		if (perIoData->isLogin) {
			// check login
			log += "401";
			strcpy(perIoData->buffer, "401 you are logged in, Please log out first!");
			writeInLogFile(log);
		}
		else {
			logIn(perIoData, log, data);
		}
	}
	else if (key == "POST") {
		if (!perIoData->isLogin) {
			// check login
			log += "402";
			strcpy(perIoData->buffer, "402 You are not log in");
			writeInLogFile(log);
		}
		else {
			postMessage(perIoData, log, data);
		}

	}
	else if (key == "QUIT") {
		if (perIoData->isLogin) {
			// check login
			log += "402";
			strcpy(perIoData->buffer, "402 You are not log in");
			writeInLogFile(log);
		}
		else {
			logOut(perIoData, log);
		}
	}
	else {
		log += "403";
		strcpy(perIoData->buffer, "403 Wrong protocol!");
		// Write in log file
		writeInLogFile(log);
	}
}

void writeInLogFile(string log) {
	EnterCriticalSection(&criticalSection);
	logFile.open("log_20183816.txt", ios::out | ios::app);
	if (logFile.is_open()) {
		logFile << log + "\n";
		logFile.close();
	}
	else cout << "Unable to open file.";
	LeaveCriticalSection(&criticalSection);
}

// Login function handle login request from client
// @param perIoData - Pointer input data and info client
// @param log - reference variable store the activity log 
// @param data - message without protocol send by client
void logIn(LPPER_IO_OPERATION_DATA perIoData, string &log, string data) {
	char rs[DATA_BUFSIZE];
	memset(rs, 0, DATA_BUFSIZE);
	EnterCriticalSection(&criticalSection);
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
				perIoData->isLogin = true;
				strcpy(perIoData->username, username.c_str());
			}
			break;
		}
	}
	if (strlen(rs) == 0) {
		strcat_s(rs, "406 User does not exist!");
		log += "406";
	}

	strcpy(perIoData->buffer, rs);
	// Write in log file
	writeInLogFile(log);
	// close file "account.txt"
	accountFile.close();
	LeaveCriticalSection(&criticalSection);
}

// postMessage function handle post message request
// @param perIoData - Pointer input data and info client
// @param log - reference variable store the activity log 
// @param data - message without protocol send by client
void postMessage(LPPER_IO_OPERATION_DATA perIoData, string &log, string data) {
	char rs[DATA_BUFSIZE];
	memset(rs, 0, DATA_BUFSIZE);

	strcat_s(rs, "200 Post sucessful!");
	log += "200";

	strcpy(perIoData->buffer, rs);
	// Write in log file
	writeInLogFile(log);
}

// Handle Log Out Request
// @param perIoData - Pointer input data and info client
// @param log - reference variable store the activity log 
void logOut(LPPER_IO_OPERATION_DATA perIoData, string &log) {
	char rs[DATA_BUFSIZE];
	memset(rs, 0, DATA_BUFSIZE);
	// Handle critical resource

	strcat_s(rs, "200 Logout sucessfull!");
	log += "200";
	perIoData->isLogin = false;
	memset(perIoData->username, 0, USER_LEN);

	strcpy(perIoData->buffer, rs);
	// Write in log file
	writeInLogFile(log);
}

// Send message
void sendMessage(char *buff, SOCKET &connectedSocket) {

	int ret = send(connectedSocket, buff, strlen(buff), 0);
	if (ret == SOCKET_ERROR) {
		printf("Error %d: Can't send data.\n", WSAGetLastError());
	}
}