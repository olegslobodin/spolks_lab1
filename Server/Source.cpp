#include "Header.h"

const int SHUTDOWN_SOCKET = 2;
const int BUFFER_SIZE = 1000;

int main()
{
	int serverSocket;

	ConfirmWinSocksDll();
	InitServerSocket(&serverSocket);
	Work(serverSocket);
	system("pause");
}

int ConfirmWinSocksDll()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		printf("ERROR: WSAStartup failed with error: %d\n", err);
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("ERROR: Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return 1;
	}

	return 0;
}

void InitServerSocket(int* serverSocket)
{
	*serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*serverSocket == SOCKET_ERROR)
	{
		PrintLastError();
		return;
	}

	sockaddr_in serverSocketParams = GetServerSocketParams();

	if (bind(*serverSocket, (LPSOCKADDR)&serverSocketParams, sizeof(serverSocketParams)) == SOCKET_ERROR)
	{
		PrintLastError();
		return;
	}

	if (listen(*serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		PrintLastError();
		return;
	}
}

sockaddr_in GetServerSocketParams()
{
	sockaddr_in sin;

	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(80);
	sin.sin_family = AF_INET;

	return sin;
}

void Work(int serverSocket)
{
	vector<int>* clientSockets = new vector<int>;
	vector<sockaddr_in>* clientSocketsParams = new vector<sockaddr_in>;

	while (true)
	{
		ConnectClient(serverSocket, clientSockets, clientSocketsParams);
		for (vector<int>::iterator clientSocket = clientSockets->begin(); clientSocket != clientSockets->end(); clientSocket++)
		{
			int clientSocketIndex = clientSocket - clientSockets->begin();
			CommandCycle(clientSocketIndex, clientSockets, clientSocketsParams);
			if (clientSockets->size() == 0)
				break;
		}
	}
}

void ConnectClient(int serverSocket, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams)
{
	sockaddr_in clientSocketParams;
	int fromlen = sizeof(clientSocketParams);

	int newClientSocket = accept(serverSocket, (struct sockaddr*)&clientSocketParams, &fromlen);
	if (newClientSocket == SOCKET_ERROR)
	{
		PrintLastError();
		return;
	}

	clientSockets->push_back(newClientSocket);
	clientSocketsParams->push_back(clientSocketParams);

	Send("Connection established!", newClientSocket);
	printf("accepted connection from %s, port %d\n", inet_ntoa(clientSocketParams.sin_addr), htons(clientSocketParams.sin_port));
}

void CommandCycle(int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams)
{
	char* buffer = (char*)calloc(BUFFER_SIZE, 1);
	int currentClientSocket = (*clientSockets)[clientIndex];

	while (true)
	{
		InputCommand(buffer, currentClientSocket);
		while (HasCommand(buffer))
		{
			string cmd = TakeNextCommand(buffer);
			cout << cmd;
			if (ProceedCommand(cmd, clientIndex, clientSockets, clientSocketsParams) != 0)
				break;
		}
	}
	free(buffer);
}

void InputCommand(char* buffer, int clientSocket)
{
	//find position of line ending
	char* currentPos = strchr(buffer, '\0');
	if (currentPos == NULL)
		currentPos = buffer;

	int bytesRecieved = recv(clientSocket, currentPos, BUFFER_SIZE - (currentPos - buffer), 0);
	if (bytesRecieved == SOCKET_ERROR)
	{
		buffer[0] = '\0';
		return;
	}
	currentPos += bytesRecieved;
	*currentPos = '\0';
}

bool HasCommand(char* buffer)
{
	return strchr(buffer, '\n') != NULL;
}

string TakeNextCommand(char* buffer)
{
	char* endLine = strchr(buffer, '\n');
	int cmdLength = endLine - buffer;
	string cmd;
	cmd.assign(buffer, cmdLength);
	for (int i = 0; i < BUFFER_SIZE - cmdLength + 1; i++)
		buffer[i] = buffer[i + cmdLength + 1];
	return cmd;
}

int ProceedCommand(string cmd, int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams)
{
	vector<string> words = split(cmd, ' ');
	int currentClientSocket = (*clientSockets)[clientIndex];

	if (words.empty())
	{
		return 0;
	}

	if (words[0].compare("echo") == 0)
	{
		if (cmd.length() > 5)
			Send(cmd.substr(5), currentClientSocket);
	}

	else if (words[0].compare("time") == 0)
		Send(GetTime(), currentClientSocket);

	else if (words[0].compare("close") == 0)
	{
		CloseConnection(clientIndex, clientSockets, clientSocketsParams);
		return SHUTDOWN_SOCKET;
	}

	return 0;
}

string CutLineEndings(char* cmd)
{
	string line;
	line.assign(cmd);
	while (!line.empty() && (line[line.length() - 1] == '\n' || line[line.length() - 1] == '\r'))
		line.erase(line.length() - 1);
	return line;
}

void CloseConnection(int clientIndex, vector<int>* clientSockets, vector<sockaddr_in>* clientSocketsParams)
{
	int currentClientSocket = (*clientSockets)[clientIndex];
	sockaddr_in currentClientSocketParams = (*clientSocketsParams)[clientIndex];

	Send("Good bye!", currentClientSocket);
	printf("Connection closed with %s, port %d\n", inet_ntoa(currentClientSocketParams.sin_addr), htons(currentClientSocketParams.sin_port));
	shutdown(currentClientSocket, SD_BOTH);

	clientSockets->erase(clientSockets->begin() + clientIndex);
	clientSocketsParams->erase(clientSocketsParams->begin() + clientIndex);
}

void Send(string str, int socket)
{
	int bufSize = str.length();
	char* buf = (char*)calloc(bufSize + 1, 1);
	strcpy(buf, str.c_str());
	if (send(socket, buf, bufSize, MSG_DONTROUTE) == -1)
		PrintLastError();
	free(buf);
}

string GetTime()
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, 80, "%d-%m-%Y %I:%M:%S", timeinfo);
	string str(buffer);

	return str;
}

void PrintLastError()
{
	wchar_t buf[256];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
	wcout << buf;
}

void split(const string &s, char delim, vector<string> &elems) {
	stringstream ss;
	ss.str(s);
	string item;
	while (getline(ss, item, delim)) {
		if (item.length())
			elems.push_back(item);
	}
}

vector<string> split(const string &s, char delim) {
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}

vector<string> split(char* char_string, char delim) {
	string s;
	s.assign(char_string);
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}