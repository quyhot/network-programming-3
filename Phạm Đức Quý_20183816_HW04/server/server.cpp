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
session client[FD_SETSIZE];

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

	printf("Server Started!\n");
}

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
	memset(clientSession->username, 0, BUFF_SIZE);


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
		logIn(data, clientSession, log);
	}
	else if (key == "POST") {
		postMessage(data, clientSession, log);
	}
	else if (key == "QUIT") {
		logOut(clientSession, log);
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
		if (ret == SOCKET_ERROR) {
			if (WSAGetLastError() == 10054) {
				printf("Client [%s:%d] disconnects. \n", clientSession->clientIP, clientSession->clientPort);
			}
			else {
				printf("Error %d: Cannot receive data.\n", WSAGetLastError());
			}
			return ret;
		}
		else if (ret == 0) {
			printf("Client [%s:%d] disconnects. \n", clientSession->clientIP, clientSession->clientPort);
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

// accept request from client
void acceptRequest() {
	clientAddr.sin_family = AF_INET;

	acceptSock = accept(listenSock, (sockaddr *)&clientAddr, &clientLength);
	if (acceptSock == SOCKET_ERROR) {
		printf("Error %d: Cannot permit incoming connection", WSAGetLastError());
		closesocket(acceptSock);
		closesocket(listenSock);
		WSACleanup();
		exit(0);
	}
}

// Communicate with client
void communicateClient(char *buff) {
	string log;
	for (int i = 0; i < FD_SETSIZE; ++i) {
		if (client[i].sock == 0) continue;
		if (FD_ISSET(client[i].sock, &readfds)) {
			// receive and handle message
			int ret = recvMessage(buff, client + i);
			if (ret <= 0) {
				client[i].isLogin = false;
				memset(client[i].username, 0, BUFF_SIZE);
				memset(client[i].clientIP, 0, BUFF_SIZE);
				client[i].clientPort = 0;
				FD_CLR(client[i].sock, &initfds);
				closesocket(client[i].sock);
				client[i].sock = 0;
			}
			else {
				buff[ret] = 0;
				printf("Receive from client [%s:%d]: %s\n", client[i].clientIP, client[i].clientPort, buff);
				// write clientIp and clientPort to log variable 
				log = client[i].clientIP;
				log += ":" + to_string(client[i].clientPort);
				// write current time to log variable 
				returnCurrentTime(log);
				// handle message
				handleProtocol(buff, client + i, log);
			}
		}
	}
}

// Initialize exploration set
void initializeExploration() {
	memset(client, 0, FD_SETSIZE);

	FD_ZERO(&initfds);
	FD_SET(listenSock, &initfds);

	char buff[BUFF_SIZE];

	while (1)
	{
		readfds = initfds; /* structure assignment */
		nEvents = select(0, &readfds, 0, 0, 0);
		if (nEvents < 0) {
			printf("Error %d: Cannot poll socket\n", WSAGetLastError());
			break;
		}

		// New client connection
		if (FD_ISSET(listenSock, &readfds)) {
			acceptRequest();
			int i;
			for (i = 0; i < FD_SETSIZE; ++i) {
				if (client[i].sock == 0) {
					client[i].sock = acceptSock;
					inet_ntop(AF_INET, &clientAddr.sin_addr, client[i].clientIP, sizeof(client[i].clientIP));
					client[i].clientPort = ntohs(clientAddr.sin_port);
					printf("Accept incoming connection from %s:%d\n", client[i].clientIP, client[i].clientPort);
					FD_SET(client[i].sock, &initfds);
					break;
				}
			}
			if (i == FD_SETSIZE) {
				printf("Too many clients.\n");
				closesocket(acceptSock);
			}
			if (--nEvents <= 0) continue; // no more event.
		}

		// Communicate with client
		communicateClient(buff);
		if (--nEvents <= 0) {
			continue;
		}
	}
}

int main(int argc, char *argv[])
{
	// Command-Line Arguments
	if (argc < 2) {
		printf("Enter error, please enter like this: server.exe 5500");
		return 0;
	}
	char* serverPort = argv[1];

	initiateWinsock();
	constructSocket();
	bindSocket(serverPort);
	listenRequest();
	initializeExploration();

	// Close Socket
	closesocket(acceptSock);
	closesocket(listenSock);

	// Terminate Winsock
	WSACleanup();
    return 0;
}

