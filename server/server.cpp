// server.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "Winsock2.h"
#include "Ws2tcpip.h"
#include "iostream"
#include "fstream"
#include "string"
#include "process.h"
#include "ctime"
#include "map"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable:4996)

#define SERVER_ADDR "127.0.0.1"
#define BUFF_SIZE 2048
#define USER_LEN 200
#define DELIMITER "\r\n"

SOCKET listenSock;
SOCKET acceptSock;
fstream accountFile;
ofstream logFile;
sockaddr_in clientAddr;
int clientLength = sizeof(clientAddr);
CRITICAL_SECTION critical;

// struct client session in server
struct account {
	SOCKET sock;
	char clientIP[INET_ADDRSTRLEN];
	int clientPort;
	char username[USER_LEN];
	bool isLogin;
} test;

map<string, bool> checkLogin;

account acc;

// Initiate Winsock
void initiateWinsock() {
	
	WSADATA wsaData;
	WORD version = MAKEWORD(2, 2);
	if (WSAStartup(version, &wsaData)) {
		printf("Winsock 2 is not supported\n");
		exit(0);
	}
}

// Construct socket
void constructSocket() {
	
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET) {
		printf("Error %d: Cannot create server socket", WSAGetLastError());
		exit(0);
	}
}

// Bind address to socket
void bindSocket(char* serverPort) {
	
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(serverPort));
	inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
	if (bind(listenSock, (sockaddr *) &addr, sizeof(addr))) {
		printf("Error %d: Cannot associate a local  address with server socket.", WSAGetLastError());
		exit(0);
	}
}

// Listen Request from client
void listenRequest() {
	
	if (listen(listenSock, 10)) {
		printf("Error %d: Cannot place server socket in state Listen", WSAGetLastError());
		exit(0);
	}

	printf("Server Started!");
}

// Send message
void sendMessage(char *rs, SOCKET &connectedSocket) {
	
	int ret = send(connectedSocket, rs, strlen(rs), 0);
	if (ret == SOCKET_ERROR) {
		printf("Error %d: Can't send data.\n", WSAGetLastError());
	}
}

// Log file
void writeInLogFile(string log) {
	logFile.open("log_20183816.txt", ios::out | ios::app);
	// Write in file log
	if (logFile.is_open()) {
		logFile << log + "\n";
		logFile.close();
	}
	else cout << "Unable to open file.";
}

// Handle Login function
void logIn(string data, account *threadAcc, string &log) {
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
			string status = line.substr(line.find(" ") + 1);
			if (status == "1") {
				strcat_s(rs, "407 User has been locked");
				log += "407";
			}
			else {
				// Handle critical resource
				EnterCriticalSection(&critical);
				if (checkLogin.find(data) == checkLogin.end()) {
					strcpy(threadAcc->username, data.c_str());
					threadAcc->isLogin = true;
					checkLogin[data] = true;
					strcat_s(rs, "200 Login successful!");
					log += "200";
				}
				else {
					if (checkLogin[data]) {
						strcat_s(rs, "401 Account is logged in somewhere!");
						log += "401";
					}
					else {
						strcpy(threadAcc->username, data.c_str());
						threadAcc->isLogin = true;
						checkLogin[data] = true;
						strcat_s(rs, "200 Login successful!");
						log += "200";
					}
				}
				LeaveCriticalSection(&critical);
				
			}
			break;
		}
	}
	if (strlen(rs) == 0) {
		strcat_s(rs, "406 User does not exist!");
		log += "406";
	}
	sendMessage(rs, threadAcc->sock);
	writeInLogFile(log);
	accountFile.close();
}

// Handle Post Message Function
void postMessage(string data, account *threadAcc, string &log) {
	char rs[BUFF_SIZE];
	memset(rs, 0, BUFF_SIZE);

	strcat_s(rs, "200 Post sucessful!");
	log += "200";
	// Send Message to Client
	sendMessage(rs, threadAcc->sock);
	writeInLogFile(log);
}

// Handle Log Out function
void logOut(account *threadAcc, string &log) {
	char rs[BUFF_SIZE];
	memset(rs, 0, BUFF_SIZE);
	// Handle critical resource
	EnterCriticalSection(&critical);
	checkLogin[threadAcc->username] = false;
	LeaveCriticalSection(&critical);

	strcat_s(rs, "200 Logout sucessfull!");
	log += "200";
	threadAcc->isLogin = false;

	// Send message
	sendMessage(rs, threadAcc->sock);
	writeInLogFile(log);
}

// Selected by the user
void chooseFunction(char *buff, account *threadAcc, string &log) {
	
	string str(buff);
	log += str + " $ ";
	string key = str.substr(0, 4);
	string data;
	if (str.length() > 4) {
		data = str.substr(5);
	}
	if (key == "USER") {
		logIn(data, threadAcc, log);
	}
	else if (key == "POST") {
		postMessage(data, threadAcc, log);
	}
	else if (key == "QUIT") {
		logOut(threadAcc, log);
	}
}

// Return current time when user send message to server
void returnCurrentTime(string &log) {
	log += "[";
	time_t current = time(0);
	tm *ltm = localtime(&current);
	log += to_string(ltm->tm_mday) + "/";
	log += to_string(ltm->tm_mon + 1) + "/";
	log += to_string(ltm->tm_year + 1900) + " ";
	log += to_string(ltm->tm_hour) + ":";
	log += to_string(ltm->tm_min) + ":";
	log += to_string(ltm->tm_sec) + "]" + " $ ";
}

// receive message from client
void recvMessage(char *buff, account *threadAcc) {
	string log;
	string buffRecv;
	memset(buff, 0, BUFF_SIZE);
	// Handle byte stream
	while (1) {
		int ret = recv(threadAcc->sock, buff, BUFF_SIZE, 0);
		if (ret == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				printf("Client [%s:%d] disconnects. \n", threadAcc->clientIP, threadAcc->clientPort);
				if (threadAcc->isLogin) {
					EnterCriticalSection(&critical);
					checkLogin[threadAcc->username] = false;
					LeaveCriticalSection(&critical);
				}
				closesocket(threadAcc->sock);
				_endthreadex(0);
			}
			else {
				printf("Error %d: Cannot receive data.\n", WSAGetLastError());
			}
		}
		else if (ret == 0) {
			printf("Client [%s:%d] disconnects. \n", threadAcc->clientIP, threadAcc->clientPort);
			if (threadAcc->isLogin) {
				EnterCriticalSection(&critical);
				checkLogin[threadAcc->username] = false;
				LeaveCriticalSection(&critical);
			}
			closesocket(threadAcc->sock);
			_endthreadex(0);
		}
		else {
			buff[ret] = 0;
			buffRecv += buff;
			if (strstr(buff, DELIMITER)) {
				buffRecv = buffRecv.substr(0, buffRecv.find(DELIMITER));
				strcpy(buff, buffRecv.c_str());
				break;
			}
		}
	}
	printf("Receive from client [%s:%d]: %s\n", threadAcc->clientIP, threadAcc->clientPort, buff);
	log = threadAcc->clientIP;
	log += ":" + to_string(threadAcc->clientPort);
	returnCurrentTime(log);
	// handle message
	chooseFunction(buff, threadAcc, log);
}

// Thread to receive and process message from client
unsigned __stdcall echoThread(void *param) {

	char buff[BUFF_SIZE];

	account threadAcc;
	threadAcc.sock = ((account*)param)->sock;
	threadAcc.clientPort = ((account*)param)->clientPort;
	strcpy(threadAcc.clientIP, ((account*)param)->clientIP);
	// communicate with client
	while (1)
	{
		recvMessage(buff, &threadAcc);
	}
	closesocket(threadAcc.sock);
	_endthreadex(0);
	return 0;

}

// accept request
void acceptRequest() {
	clientAddr.sin_family = AF_INET;

	acceptSock = accept(listenSock, (sockaddr *)&clientAddr, &clientLength);
	if (acceptSock == SOCKET_ERROR) {
		printf("Error %d: Cannot permit incoming connection", WSAGetLastError());
		exit(0);
	}
	else {
		inet_ntop(AF_INET, &clientAddr.sin_addr, acc.clientIP, sizeof(acc.clientIP));
		acc.clientPort = ntohs(clientAddr.sin_port);
		printf("Accept incoming connection from %s:%d\n", acc.clientIP, acc.clientPort);
		acc.sock = acceptSock;
		_beginthreadex(0, 0, echoThread, (void *) &acc, 0, 0);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Enter error, please enter like this: server.exe 5500");
		return 0;
	}
	char* serverPort = argv[1];

	// Init critical
	InitializeCriticalSection(&critical);

	initiateWinsock();
	constructSocket();
	bindSocket(serverPort);
	listenRequest();
	while (1) {
		acceptRequest();
	}

	// Release resources used by the critical section object.
	DeleteCriticalSection(&critical);

	// Close Socket
	closesocket(acceptSock);
	closesocket(listenSock);

	// Terminate Winsock
	WSACleanup();
    return 0;
}

