#pragma once

#include <Ws2tcpip.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <string>
#include <winsock2.h>
#include <string.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#define SocketPort 27015
#define	 WIN32_LEAN_AND_MEAN

using namespace std;

int ConfirmWinSocksDll();

void InitServerSocket(int* serverSocket);

sockaddr_in GetServerSocketParams();

void Work(int serverSocket);

void ConnectClient(int serverSocket, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams);

void CommandCycle(int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams);

void InputCommand(char* recvBuffer, int clientSocket);

bool HasCommand(char* buffer);

string TakeNextCommand(char* buffer);

int ProceedCommand(string cmd, int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams);

string CutLineEndings(char* cmd);

void CloseConnection(int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams);

void Send(string str, int socket);

string GetTime();

void PrintLastError();

void split(const string &s, char delim, vector<string> &elems);

vector<string> split(const string &s, char delim);

vector<string> split(char* char_string, char delim);