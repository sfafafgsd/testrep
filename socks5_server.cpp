#include <WinSock2.h>
#include <ws2tcpip.h>
#include "socks5_server.hpp"

#include <thread>
#include <stdexcept>

#include "winapi_raii.hpp"

socks5_server::socks5_server() : m_server_socket(0)
{
	m_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == m_server_socket)
		throw std::runtime_error("socket creation error");
}

socks5_server::~socks5_server()
{
	if (m_server_socket) {
		closesocket(m_server_socket);
		m_server_socket = 0;
	}
}

bool auth_client(SOCKET clientsock) {
	client_hello cl_hello;

	int sz = recv(clientsock, reinterpret_cast<char*>(&cl_hello), sizeof(cl_hello), 0);
	if (sz != sizeof(cl_hello) || cl_hello.socks_ver != 5 || !cl_hello.count_auth_methods) {
		return false;
	}
	bool can_proxy_without_auth = false;

	std::unique_ptr<byte[]> methods = std::make_unique<byte[]>(cl_hello.count_auth_methods);
	sz = recv(clientsock, reinterpret_cast<char*>(methods.get()), cl_hello.count_auth_methods, 0);

	if (sz != cl_hello.count_auth_methods)
	{
		return false;
	}

	for (int i = 0; i < cl_hello.count_auth_methods; ++i) {
		if (methods[i] == 0) {
			can_proxy_without_auth = true;
			break;
		}
	}

	if (!can_proxy_without_auth) {
		return false;
	}

	server_hello s_hello;
	return SOCKET_ERROR != send(clientsock, reinterpret_cast<const char*>(&s_hello), sizeof(s_hello), 0);
}

SOCKET connect2clients_host(const client_request& cl_request, SOCKET clientsock) {
	SOCKET relay_socket = 0;
	HOST_ERROR error = HOST_ERROR::SUCC;
	dst_ipv4 dst4 = { 0 };
	dst_ipv6 dst6 = { 0 };

	bool using_ipv4 = true;

	switch (cl_request.command_code) {
	case COMMAND_CODES::TCP_IP:
	{
		switch (cl_request.addr_type) {
		case ADDRESS_TYPE::IPv4: {
			int sz = recv(clientsock, reinterpret_cast<char*>(&dst4), sizeof(dst4), 0);
			if (sz != sizeof(dst4))
			{
				return relay_socket;
			}

			relay_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (relay_socket == INVALID_SOCKET)
			{
				relay_socket = 0;
				return relay_socket;
			}

			sockaddr_in sin;
			memcpy(&sin.sin_addr, &dst4.addr, sizeof(dst4.addr));
			sin.sin_family = AF_INET;
			sin.sin_port = dst4.port;

			if (connect(relay_socket, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin))) {
				closesocket(relay_socket);
				relay_socket = 0;
				error = HOST_ERROR::HOST_UNREACHABLE;
				return relay_socket;
			}

			break;
		}
		case ADDRESS_TYPE::IPv6: {
			using_ipv4 = false;
			int sz = recv(clientsock, reinterpret_cast<char*>(&dst6), sizeof(dst6), 0);
			if (sz != sizeof(dst6))
			{
				return relay_socket;
			}

			relay_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			if (relay_socket == INVALID_SOCKET)
			{
				relay_socket = 0;
				return relay_socket;
			}

			sockaddr_in6 sin;
			memcpy(&sin.sin6_addr, &dst6.addr, sizeof(dst6.addr));
			sin.sin6_family = AF_INET6;
			sin.sin6_port = dst6.port;

			if (connect(relay_socket, reinterpret_cast<const sockaddr*>(&sin), sizeof(sin))) {
				closesocket(relay_socket);
				relay_socket = 0;
				error = HOST_ERROR::HOST_UNREACHABLE;
				return relay_socket;
			}
			break;
		}
		default:
			error = HOST_ERROR::ADDRESS_INVALID;
			break;
		}
		break;
	}
	case COMMAND_CODES::TCP_IP_BINDING:
	{
		error = HOST_ERROR::BAD_COMMAND; // не обрабатывать
		break;
	}
	case COMMAND_CODES::UDP:
	{
		error = HOST_ERROR::BAD_COMMAND; // не обрабатывать
		break;
	}
	default:
		error = HOST_ERROR::BAD_COMMAND;
		break;
	}

	server_response s_resp(error, cl_request.addr_type);
	send(clientsock, reinterpret_cast<const char*>(&s_resp), sizeof(s_resp), 0);
	if (using_ipv4)
		send(clientsock, reinterpret_cast<const char*>(&dst4), sizeof(dst4), 0);
	else
		send(clientsock, reinterpret_cast<const char*>(&dst6), sizeof(dst6), 0);

	return relay_socket;
}

void handle_client(LPVOID _pth) {
	SOCKET_raii clientsock(reinterpret_cast<SOCKET>(_pth));
	if (!auth_client(clientsock.get())) {
		ExitThread(0); //return;
	}

	client_request cl_request;
	int sz = recv(clientsock.get(), reinterpret_cast<char*>(&cl_request), sizeof(cl_request), 0);
	if (sz != sizeof(cl_request)) {
		ExitThread(0); //return;
	}

	if (cl_request.socks_ver != 5) {
		ExitThread(0); //return;
	}

	SOCKET _relay_socket = connect2clients_host(cl_request, clientsock.get());

	if (_relay_socket) {
		SOCKET_raii relay_socket(_relay_socket);
		HANDLE _hEvents[2] = { 0 };
		_hEvents[0] = CreateEventW(NULL, FALSE, FALSE, NULL);
		_hEvents[1] = CreateEventW(NULL, FALSE, FALSE, NULL);

		if (!_hEvents[0]) {
			return;
		}

		if (!_hEvents[1]) {
			CloseHandle(_hEvents[0]);
			return;
		}

		HANDLE_raii hEvents[2] = { _hEvents[0], _hEvents[1] };

		WSAEventSelect(clientsock.get(), hEvents[0].get(), FD_READ | FD_WRITE);
		WSAEventSelect(relay_socket.get(), hEvents[1].get(), FD_READ | FD_WRITE);

		bool isRunning = true;
		int sent = 0;

		std::unique_ptr<char[]> buffer = std::make_unique<char[]>(BUFFER_SIZE);
		while (isRunning) {
			DWORD completed_event = WaitForMultipleObjects(2, _hEvents, FALSE, WAIT_TIME) - WAIT_OBJECT_0;
			if (completed_event < 0 || completed_event > 1)
			{
				isRunning = false;
			}
			
			switch (completed_event)
			{
			case 0:
			{
				sent = 0;
				int received = recv(clientsock.get(), buffer.get(), BUFFER_SIZE, 0);
				if (!received) {
					return;
				}

				while (sent < received) {
					int tmp = send(relay_socket.get(), buffer.get() + sent, received - sent, 0);
					if (tmp == SOCKET_ERROR) {
						DWORD err = WSAGetLastError();
						if (err != 10035) {
							isRunning = false;
							break;
						}
						else {
							WaitForSingleObject(hEvents[1].get(), 20);
						}
					}
					else {
						sent += tmp;
					}
				}

				break;
			}
			case 1: {
				sent = 0;
				int received = recv(relay_socket.get(), buffer.get(), BUFFER_SIZE, 0);
				if (!received) {
					return;
				}

				while (sent < received) {
					int tmp = send(clientsock.get(), buffer.get() + sent, received - sent, 0);
					if (tmp == SOCKET_ERROR) {
						DWORD err = WSAGetLastError();
						if (err != 10035) {
							isRunning = false;
							break;
						}
						else {
							WaitForSingleObject(hEvents[0].get(), 20);
						}
					}
					else {
						sent += tmp;
					}
				}
				break;
			}
			default:
				break;
			}
		}
	}
}

bool socks5_server::start()
{
	if (!bind_listen())
		return false;

	while (true) {
		SOCKET client_socket = accept(m_server_socket, nullptr, nullptr);
		if (client_socket == -1) {
			return false;
		}

		HANDLE h = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(handle_client), reinterpret_cast<LPVOID>(client_socket), 0, 0); 
		if (h) 
			CloseHandle(h);
	}

	return true;
}

bool socks5_server::bind_listen()
{
	if (!m_server_socket)
		return false;

	sockaddr_in srv = { 0 };
	srv.sin_family = AF_INET;
	srv.sin_port = htons(socks5_port);
	inet_pton(srv.sin_family, host_ip, &srv.sin_addr);

	if (bind(m_server_socket, reinterpret_cast<const sockaddr*>(&srv), sizeof(srv)) == SOCKET_ERROR)
		return false;

	if (listen(m_server_socket, SOMAXCONN) == SOCKET_ERROR)
		return false;

	return true;
}
