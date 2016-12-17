#include "Header.h"

const int SHUTDOWN_SOCKET = 2;
const int MAX_FLAG_STRING_LENGTH = 60;
const int FILE_BUFFER_SIZE = 10 * 1024 * 1024;
const int TRANSFER_BLOCK_SIZE = 10 * 1024 * 1024 - 2 * MAX_FLAG_STRING_LENGTH; // - 2*MAX_... to avoid receive buffer overflow
const char *END_OF_FILE = "File sent 8bb20328-3a19-4db8-b138-073a48f57f4a";
const char *FILE_SEND_ERROR = "File send error 8bb20328-3a19-4db8-b138-073a48f57f4a";
const char *FILE_NOT_FOUND = "File is not found 8bb20328-3a19-4db8-b138-073a48f57f4a";

#if  defined _WIN32 || defined _WIN64
const char *STORE_PATH = "../debug/store/";
#elif defined __linux__
const char *STORE_PATH = "./store/";
#endif

int main() {
	int serverSocket;

#if  defined _WIN32 || defined _WIN64
	ConfirmWinSocksDll();
#endif

	InitServerSocket(&serverSocket);
	Work(serverSocket);

#if  defined _WIN32 || defined _WIN64
	system("pause");
#endif
}

#if  defined _WIN32 || defined _WIN64
int ConfirmWinSocksDll() {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		printf("ERROR: WSAStartup failed with error: %d\n", err);
		return 1;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("ERROR: Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return 1;
	}

	return 0;
}
#endif

void InitServerSocket(int *serverSocket) {
	*serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*serverSocket == SOCKET_ERROR) {
		PrintLastError();
		return;
	}

	sockaddr_in serverSocketParams = GetServerSocketParams();

#if  defined _WIN32 || defined _WIN64
	if (bind(*serverSocket, (LPSOCKADDR)& serverSocketParams, sizeof(serverSocketParams)) == SOCKET_ERROR) {
		PrintLastError();
		return;
	}
#elif defined __linux__
	if (bind(*serverSocket, (sockaddr *)&serverSocketParams, sizeof(serverSocketParams)) == SOCKET_ERROR) {
		PrintLastError();
		return;
	}
#endif

	if (listen(*serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		PrintLastError();
		return;
	}
}

sockaddr_in GetServerSocketParams() {
	sockaddr_in sin;

	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(10000);
	sin.sin_family = AF_INET;

	return sin;
}

void Work(int serverSocket) {
	vector<Client> *clients = new vector<Client>;

	while (true) {
		ConnectClient(serverSocket, clients);
		for (vector<Client>::iterator client = clients->begin();
			client != clients->end(); client++) {
			int clientIndex = client - clients->begin();
			CommandCycle(clientIndex, clients);
			if (clients->size() == 0)
				break;
		}
	}
}

void ConnectClient(int serverSocket, vector<Client> *clients) {
	sockaddr_in clientSocketParams;

#if  defined _WIN32 || defined _WIN64
	int fromlen = sizeof(clientSocketParams);
#elif defined __linux__
	socklen_t fromlen = sizeof(clientSocketParams);
#endif

	int newClientSocket = accept(serverSocket, (struct sockaddr *) &clientSocketParams, &fromlen);
	if (newClientSocket == SOCKET_ERROR) {
		PrintLastError();
		return;
	}

	Client newClient(newClientSocket, clientSocketParams);

	clients->push_back(newClient);

	SendString("Connection established!\n", newClientSocket);
	printf("accepted connection from %s, port %d\n", inet_ntoa(clientSocketParams.sin_addr),
		htons(clientSocketParams.sin_port));
}

void CommandCycle(int clientIndex, vector<Client> *clients) {
	bool breakCycle = false;
	Client client = (*clients)[clientIndex];

	while (!breakCycle) {
		InputCommand(client.Buffer, client.Socket);
		while (HasCommand(client.Buffer)) {
			string cmd = TakeNextCommand(client.Buffer);
			cout << cmd << endl;
			if (ProceedCommand(cmd, clientIndex, clients) != 0) {
				breakCycle = true;
				break;
			}
		}
	}
}

void InputCommand(char *buffer, int clientSocket) {
	//find position of line ending
	char *currentPos = strchr(buffer, '\0');
	if (currentPos == NULL)
		currentPos = buffer;

	int bytesRecieved = recv(clientSocket, currentPos, Client::BUFFER_SIZE - (currentPos - buffer), 0);
	if (bytesRecieved == SOCKET_ERROR) {
		buffer[0] = '\0';
		return;
	}
	currentPos += bytesRecieved;
	*currentPos = '\0';
}

bool HasCommand(char *buffer) {
	return strchr(buffer, '\n') != NULL;
}

string TakeNextCommand(char *buffer) {
	char *endLine = strchr(buffer, '\n');
	int cmdLength = endLine - buffer;
	string cmd;
	cmd.assign(buffer, cmdLength);
	for (int i = 0; i < Client::BUFFER_SIZE - cmdLength + 1; i++)
		buffer[i] = buffer[i + cmdLength + 1];
	return cmd;
}

int ProceedCommand(string cmd, int clientIndex, vector<Client> *clients) {
	vector<string> words = split(cmd, ' ');
	int currentClientSocket = (*clients)[clientIndex].Socket;

	if (words.empty()) {
		return 0;
	}

	if (words[0].compare("echo") == 0) {
		if (cmd.length() > 5)
			SendString(cmd.substr(5) + '\n', currentClientSocket);
	}

	else if (words[0].compare("time") == 0) {
		SendString(GetTime() + '\n', currentClientSocket);
	}

	else if (words[0].compare("send") == 0)
	{
		if (words.size() > 1) {
			ReceiveFile(currentClientSocket, words[1]);
		}
	}

	else if (words[0].compare("receive") == 0)
	{
		if (words.size() > 1) {
			SendFile(currentClientSocket, words[1]);
		}
	}

	else if (words[0].compare("close") == 0) {
		CloseConnection(clientIndex, clients);
		return SHUTDOWN_SOCKET;
	}

	else {
		SendString("Command not found!\n", currentClientSocket);
	}

	return 0;
}

void ReceiveFile(int socket, string fileName) {
	ofstream file;
	file.open(STORE_PATH + fileName, ios::binary);

	if (file.is_open()) {
		SendString("ready\n", socket);
	}
	else {
		SendString("Can't open file for writing on server side. Error #" + errno + '\n', socket);
		return;
	}

	char *fileBuffer = (char*)calloc(FILE_BUFFER_SIZE, 1);
	unsigned long long totalBytesReceived = 0;
	bool sendingComplete = false;

	while (!sendingComplete)
	{
		unsigned long long recievedBytesCount = recv(socket, fileBuffer, FILE_BUFFER_SIZE, 0);
		if (Contains(fileBuffer, recievedBytesCount, END_OF_FILE)) {
			SendString("File sent successfully\n", socket);
			sendingComplete = true;
			recievedBytesCount -= strlen(END_OF_FILE);
		}
		if (Contains(fileBuffer, recievedBytesCount, FILE_SEND_ERROR)) {
			SendString("File sending was aborted\n", socket);
			break;
		}
		else if (recievedBytesCount == SOCKET_ERROR) {
			SendString("Can't receive file on server side. Error #" + errno + '\n', socket);
			PrintLastError();
			break;
		}
		file.write(fileBuffer, recievedBytesCount);
		totalBytesReceived += recievedBytesCount;
		cout << "\r" << totalBytesReceived << " bytes received";
	}
	free(fileBuffer);
	file.close();
	cout << endl << "Receiving complete" << endl;
}

void SendFile(int socket, string fileName)
{
	ifstream file;
	file.open(STORE_PATH + fileName, ios::binary);
	if (!file.is_open()) {
		SendString(FILE_NOT_FOUND, socket);
		return;
	}

	char *fileBuffer = (char*)calloc(TRANSFER_BLOCK_SIZE, 1);
	unsigned long long pos = 0, length = 0;
	file.seekg(0, ios::end);
	unsigned long long fileSize = file.tellg();
	file.seekg(0, ios::beg);

	do {
		if (fileSize - pos < TRANSFER_BLOCK_SIZE)
			length = fileSize - pos;
		else
			length = TRANSFER_BLOCK_SIZE;
		file.read(fileBuffer, length);
		cout << "\r" << file.tellg() << " bytes read";
		pos = file.tellg();
		if (length > 0)
			send(socket, fileBuffer, length, 0);
	} while (length > 0);
	file.close();

	SendString(END_OF_FILE, socket);

	free(fileBuffer);
	cout << endl << "Sending complete\n" << endl;
}

//int SendSerial(int socket, const char* buffer, int length, int flags)
//{
//	for (int i = 0; i < length; i++)
//		if (send(socket, buffer + i, 1, flags) == SOCKET_ERROR)
//			return SOCKET_ERROR;
//	return 0;
//}

bool Contains(char *buffer, int bufferLength, const char *substring)
{
	int bufferPos = 0;
	while (bufferPos < bufferLength)
	{
		int subStringPos = 0;
		while (bufferPos < bufferLength && buffer[bufferPos] == substring[subStringPos])
		{
			bufferPos++;
			if (++subStringPos == strlen(substring))
				return true;
		}
		bufferPos++;
	}
	return false;
}

void CloseConnection(int clientIndex, vector<Client> *clients) {
	int currentClientSocket = (*clients)[clientIndex].Socket;
	sockaddr_in currentClientSocketParams = (*clients)[clientIndex].SocketParams;

	SendString("Good bye!\n", currentClientSocket);
	printf("Connection closed with %s, port %d\n", inet_ntoa(currentClientSocketParams.sin_addr),
		htons(currentClientSocketParams.sin_port));
	shutdown(currentClientSocket, SD_BOTH);

	(*clients)[clientIndex].Dispose();
	clients->erase(clients->begin() + clientIndex);
}

void SendString(string str, int socket) {
	int bufSize = str.length();
	char *buf = (char *)calloc(bufSize + 1, 1);
	strcpy(buf, str.c_str());
	if (send(socket, buf, bufSize, MSG_DONTROUTE) == -1)
		PrintLastError();
	free(buf);
}

string GetTime() {
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, 80, "%d-%m-%Y %I:%M:%S", timeinfo);
	string str(buffer);

	return str;
}

void PrintLastError() {
#if  defined _WIN32 || defined _WIN64
	wchar_t buf[256];
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
	wcout << buf;
#elif defined __linux__
	int error = errno;
	cout << strerror(error);
#endif
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

vector<string> split(char *char_string, char delim) {
	string s;
	s.assign(char_string);
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}