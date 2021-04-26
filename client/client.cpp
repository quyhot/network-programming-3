// 2.5.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "winsock2.h"
#include "Ws2tcpip.h"
#include "string"
#include "iostream"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable:4996)

#define DELIMITER "\r\n"
#define BUFF_SIZE 2048

SOCKET clientSock;
sockaddr_in serverAddr;
char buff[BUFF_SIZE], buffIn[BUFF_SIZE];
int ret, messageLength;
bool isLogin = false;
string str, key;

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
	
	clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSock == INVALID_SOCKET) {
		printf("Error %d: Cannot create client socket", WSAGetLastError());
		exit(0);
	}
}

// Request connect to server
void requestConnectToServer(char *serverIP, char *serverPort) {
	// Specify server address
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(serverPort));
	inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

	// Request connect to server
	if (connect(clientSock, (sockaddr *) &serverAddr, sizeof(serverAddr))) {
		printf("Error %d: Cannot connect server.", WSAGetLastError());
		exit(0);
	}
	
	printf("Connected Server!\n");
}

// Send message to server
void sendMessage() {
	
	strcat_s(buff, DELIMITER);
	messageLength = strlen(buff);
	int index = 0;
	// Handle byte stream
	while (messageLength > 0) {
		memcpy(buff, buff + index, messageLength);
		ret = send(clientSock, buff, messageLength, 0);
		if (ret == SOCKET_ERROR) {
			printf("Error %d: Cannot send data.\n", WSAGetLastError());
		}
		messageLength -= ret;
		index += ret;
	}
}

void recvMessage() {
	// Receive echo message
	ret = recv(clientSock, buff, BUFF_SIZE, 0);
	if (ret == SOCKET_ERROR) {
		if (WSAGetLastError() == WSAETIMEDOUT) {
			printf("Time out!");
		}
		else {
			printf("Error %d: Cannot receive data.\n", WSAGetLastError());
		}
	}
	else if (strlen(buff) > 0) {
		// echo message
		buff[ret] = 0;
	}
}

/*
void communicateServer() {
	while (1)
	{
		memset(buff, 0, BUFF_SIZE);
		memset(buffIn, 0, BUFF_SIZE);
		// Enter string
		printf("Send to server: ");
		gets_s(buff, BUFF_SIZE);
		messageLength = strlen(buff);
		if (messageLength == 0) break;

		// Send message
		sendMessage();
		// Receive echo message
		recvMessage();
	}
}
*/

// login function
void login() {
	while (1)
	{
		if (isLogin) {
			printf("you are logged in, Please log out first!\n\n");
			break;
		}
		memset(buff, 0, BUFF_SIZE);
		memset(buffIn, 0, BUFF_SIZE);
		// Enter string
		printf("To back to menu, please press enter!\n");
		printf("Please enter username!\n\n");
		printf("USER: ");
		gets_s(buffIn, BUFF_SIZE);

		if (strlen(buffIn) == 0) break;
		strcat_s(buff, "USER ");
		strcat_s(buff, buffIn);

		// Send message
		sendMessage();
		// Receive message
		recvMessage();

		// handle echo message from server
		str = buff;
		key = str.substr(0, 3);
		if (key == "200") {
			cout << str.substr(4) << endl;
			cout << endl;
			isLogin = true;
			break;
		}
		else if (key == "406") {
			cout << str.substr(4) << endl;
			cout << endl;
		}
		else if (key == "401") {
			cout << str.substr(4) << endl;
			cout << endl;
		}
		else if (key == "407") {
			cout << str.substr(4) << endl;
			cout << endl;
		}
	}
}

// Post Message function
void postMessage() {
	while (1)
	{
		// Check login
		if (!isLogin) {
			cout << "You are not log in!" << endl << endl;
			break;
		}
		memset(buff, 0, BUFF_SIZE);
		memset(buffIn, 0, BUFF_SIZE);
		// Enter String
		cout << "To back to menu, please press enter!" << endl;
		cout << "Enter message!\n\n";
		cout << "POST: ";
		gets_s(buffIn, BUFF_SIZE);
		
		if (strlen(buffIn) == 0) break;
		strcat_s(buff, "POST ");
		strcat_s(buff, buffIn);

		// Send Message
		sendMessage();
		// Receive message
		recvMessage();

		// handle echo message from server
		str = buff;
		key = str.substr(0, 3);
		if (key == "200") {
			cout << str.substr(4) << endl << endl;
		}

	}
}

// Log out function
void logOut() {
	if (!isLogin) {
		// Check login
		cout << "You are not log in!" << endl << endl;
		return;
	}
	else {
		memset(buff, 0, BUFF_SIZE);
		strcat_s(buff, "QUIT");

		// Send message
		sendMessage();

		// Receive Message
		recvMessage();
		
		// handle echo message from server
		str = buff;
		key = str.substr(0, 3);
		if (key == "200") {
			cout << str.substr(4) << endl << endl;
			isLogin = false;
		}
	}
}

// Menu function
void menu() {
	while (1) {
		// display menu
		printf("1. Log in\n2. Post message\n3. Logout\n4. Exit\n");
		printf("Choose a function: ");
		char choose[BUFF_SIZE];
		// choose a function
		gets_s(choose, BUFF_SIZE);
		switch (atoi(choose)) {
		case 1:
			login();
			break;
		case 2:
			postMessage();
			break;
		case 3:
			logOut();
			break;
		case 4:
			closesocket(clientSock);
			WSACleanup();
			exit(1);
		default:
			printf("Enter error: please enter again!\n\n");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Enter error, please enter like this: client.exe 127.0.0.1 5500");
		return 0;
	}
	char *serverIP = argv[1];
	char *serverPort = argv[2];

	initiateWinsock();
	constructSocket();
	requestConnectToServer(serverIP, serverPort);
	menu();

	// Close socket
	closesocket(clientSock);

	// Terminate winsock
	WSACleanup();

    return 0;
}

