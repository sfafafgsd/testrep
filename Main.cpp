#include <Windows.h>
#include "socks5_server.hpp"

#pragma comment(lib, "Ws2_32.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	WSADATA wdata;
	if (WSAStartup(MAKEWORD(2, 2), &wdata))
		ExitProcess(0);

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	socks5_server server;
	server.start();


	WSACleanup();
	ExitProcess(0);
}