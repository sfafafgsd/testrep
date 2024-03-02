#pragma once

#include "socks5_defs.hpp"
#include <Windows.h>

#define WAIT_TIME 20 * 1000 // 20 сек ждать данные
#define BUFFER_SIZE 4096 // "пропускная способность" по 4 кб. Больше 4 кб не имеет смысла ставить.

class socks5_server
{
public:
	socks5_server();
	~socks5_server();
	bool start();

private:
	const ushort socks5_port = 13337;
	const char* host_ip = "0.0.0.0"; // все интерфейсы
	SOCKET m_server_socket;

	bool bind_listen();
};

