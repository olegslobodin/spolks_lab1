#pragma once

#include <stdio.h>
#include <time.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>

#if  defined _WIN32 || defined _WIN64

#include <Ws2tcpip.h>
#include <winsock2.h>
#define SocketPort 27015
#define WIN32_LEAN_AND_MEAN

#elif defined __linux__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <errno.h>
#define SOCKET_ERROR -1
#define SD_BOTH 2

#endif

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct Client
{
	int Socket;
	sockaddr_in SocketParams;

	Client(int socket, sockaddr_in socketParams)
	{
		Socket = socket;
		SocketParams = socketParams;
	}
};

int ConfirmWinSocksDll();

void InitServerSocket(int *serverSocket);

sockaddr_in GetServerSocketParams();

void Work(int serverSocket);

void ConnectClient(int serverSocket, vector<Client> *clients);

void CommandCycle(int clientIndex, vector<Client> *clients);

void InputCommand(char *recvBuffer, int clientSocket);

bool HasCommand(char *buffer);

string TakeNextCommand(char *buffer);

int ProceedCommand(string cmd, int clientIndex, vector<Client> *clients);

string CutLineEndings(char *cmd);

void CloseConnection(int clientIndex, vector<Client> *clients);

void Send(string str, int socket);

string GetTime();

void PrintLastError();

void split(const string &s, char delim, vector<string> &elems);

vector<string> split(const string &s, char delim);

vector<string> split(char *char_string, char delim);