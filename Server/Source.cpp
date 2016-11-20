#include "Header.h"

const int SHUTDOWN_SOCKET = 2;
const int BUFFER_SIZE = 1000;

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
    if (bind(*serverSocket, (LPSOCKADDR) & serverSocketParams, sizeof(serverSocketParams)) == SOCKET_ERROR) {
        PrintLastError();
        return;
    }
#elif defined __linux__
    if (bind(*serverSocket, (sockaddr *) &serverSocketParams, sizeof(serverSocketParams)) == SOCKET_ERROR) {
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

    Send("Connection established!", newClientSocket);
    printf("accepted connection from %s, port %d\n", inet_ntoa(clientSocketParams.sin_addr),
           htons(clientSocketParams.sin_port));
}

void CommandCycle(int clientIndex, vector<Client> *clients) {
    char *buffer = (char *) calloc(BUFFER_SIZE, 1);
    bool breakCycle = false;

    while (!breakCycle) {
        InputCommand(buffer, (*clients)[clientIndex].Socket);
        while (HasCommand(buffer)) {
            string cmd = TakeNextCommand(buffer);
            cout << cmd << endl;
            if (ProceedCommand(cmd, clientIndex, clients) != 0) {
                breakCycle = true;
                break;
            }
        }
    }
    free(buffer);
}

void InputCommand(char *buffer, int clientSocket) {
    //find position of line ending
    char *currentPos = strchr(buffer, '\0');
    if (currentPos == NULL)
        currentPos = buffer;

    int bytesRecieved = recv(clientSocket, currentPos, BUFFER_SIZE - (currentPos - buffer), 0);
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
    for (int i = 0; i < BUFFER_SIZE - cmdLength + 1; i++)
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
            Send(cmd.substr(5), currentClientSocket);
    } else if (words[0].compare("time") == 0)
        Send(GetTime(), currentClientSocket);

    else if (words[0].compare("close") == 0) {
        CloseConnection(clientIndex, clients);
        return SHUTDOWN_SOCKET;
    } else {
        Send("Command not found!", currentClientSocket);
    }

    return 0;
}

string CutLineEndings(char *cmd) {
    string line;
    line.assign(cmd);
    while (!line.empty() && (line[line.length() - 1] == '\n' || line[line.length() - 1] == '\r'))
        line.erase(line.length() - 1);
    return line;
}

void CloseConnection(int clientIndex, vector<Client> *clients) {
    int currentClientSocket = (*clients)[clientIndex].Socket;
    sockaddr_in currentClientSocketParams = (*clients)[clientIndex].SocketParams;

    Send("Good bye!", currentClientSocket);
    printf("Connection closed with %s, port %d\n", inet_ntoa(currentClientSocketParams.sin_addr),
           htons(currentClientSocketParams.sin_port));
    shutdown(currentClientSocket, SD_BOTH);

    clients->erase(clients->begin() + clientIndex);
}

void Send(string str, int socket) {
    int bufSize = str.length();
    char *buf = (char *) calloc(bufSize + 1, 1);
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