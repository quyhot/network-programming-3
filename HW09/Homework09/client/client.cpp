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
int ret;

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
	if (connect(clientSock, (sockaddr *)&serverAddr, sizeof(serverAddr))) {
		printf("Error %d: Cannot connect server.", WSAGetLastError());
		exit(0);
	}

	printf("Connected Server!\n");
}

// Send message to server
void sendMessage(char *buff, int messageLength) {
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
// Receive message and echo message from server
void recvMessage(char *buff) {
	ret = recv(clientSock, buff, BUFF_SIZE, 0);
	if (ret == SOCKET_ERROR) {
		printf("Error %d: Cannot receive data.\n", WSAGetLastError());

	}
	else if (strlen(buff) > 0) {
		buff[ret] = 0;
		string str = buff;
		cout << str.substr(4) << endl;
		cout << endl;
	}
}

// login function
void login() {
	while (1) {
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
		strcat_s(buff, DELIMITER);

		// Send message
		sendMessage(buff, strlen(buff));
		// Receive message
		recvMessage(buff);
		string recvData = buff;
		string key = recvData.substr(0, 3);
		if (key == "200") break;
		else if (key == "401") break;
	}
}

// Post Message function
void postMessage() {
	while (1)
	{
		// Check login
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
		strcat_s(buff, DELIMITER);

		// Send message
		sendMessage(buff, strlen(buff));
		// Receive message
		recvMessage(buff);
		string recvData = buff;
		string key = recvData.substr(0, 3);
		if (key != "200") break;
	}
}

// Log out function
void logOut() {
	memset(buff, 0, BUFF_SIZE);
	strcat_s(buff, "QUIT");
	strcat_s(buff, DELIMITER);

	// Send message
	sendMessage(buff, strlen(buff));
	// Receive message
	recvMessage(buff);
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
	// Command-Line Arguments
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
