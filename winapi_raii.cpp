#include "winapi_raii.hpp"

HANDLE_raii::HANDLE_raii(HANDLE handle) : OBJECT_raii<HANDLE>(handle) {}

HANDLE_raii::~HANDLE_raii()
{
	if (m_object && m_object != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_object);
		m_object = 0;
	}
}

SOCKET_raii::SOCKET_raii(SOCKET handle) : OBJECT_raii<SOCKET>(handle) {}

SOCKET_raii::~SOCKET_raii()
{
	if (m_object && m_object != SOCKET_ERROR) {
		closesocket(m_object);
		m_object = 0;
	}
}
