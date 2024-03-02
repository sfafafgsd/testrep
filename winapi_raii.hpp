#pragma once

#include <Windows.h>

template <typename T>
class OBJECT_raii {
protected:
	T m_object;
public:
	OBJECT_raii(const T& object) : m_object(object) {}
	virtual ~OBJECT_raii() {}
	inline T get() const {
		return m_object;
	}
};

class HANDLE_raii : public OBJECT_raii<HANDLE> {
public:
	HANDLE_raii(HANDLE handle);
	~HANDLE_raii();
};

class SOCKET_raii : public OBJECT_raii<SOCKET> {
public:
	SOCKET_raii(SOCKET handle);
	~SOCKET_raii();
};