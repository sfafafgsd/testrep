#pragma once

typedef unsigned char byte;
typedef unsigned short ushort;

#pragma pack(push, 1)
struct client_hello {
	byte socks_ver; // 5
	byte count_auth_methods;
	// массив из методов аутентификации. Его динамически выделить (кол-во count_auth_methods)
};
#pragma pack(pop)

#pragma pack(push, 1)
struct server_hello {
	byte socks_ver; // 5
	byte auth_method; // 0 - без кредов

	server_hello();
};
#pragma pack(pop)

enum class COMMAND_CODES : byte {
	TCP_IP = 1,
	TCP_IP_BINDING = 2,
	UDP = 3
};

enum class ADDRESS_TYPE : byte {
	IPv4 = 1,
	DOMAIN_NAME = 3,
	IPv6 = 6
};

#pragma pack(push, 1)
struct client_request {
	byte socks_ver; // 5
	COMMAND_CODES command_code;
	byte reserved;
	ADDRESS_TYPE addr_type;
};
#pragma pack(pop)

union addr_ipv4 {
	struct {
		byte first;
		byte second;
		byte third;
		byte fourth;
	};
	unsigned int all;
};

#pragma pack(push, 1)
struct dst_ipv4 {
	addr_ipv4 addr;
	ushort port;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct dst_ipv6 {
	byte addr[16];
	ushort port;
};
#pragma pack(pop)

// dst_domain делать вручную

enum class HOST_ERROR : byte {
	SUCC = 0,
	SOCKS_ERROR = 1,
	RULE_DENIED = 2,
	NET_UNREACHABLE = 3,
	HOST_UNREACHABLE = 4,
	CONNECTION_DENIED = 5,
	TTL = 6,
	BAD_COMMAND = 7,
	ADDRESS_INVALID = 8
};

#pragma pack(push, 1)
struct server_response {
	byte socks_ver; // 5
	HOST_ERROR error;
	byte reserved;
	ADDRESS_TYPE addr_type;

	// dst host info

	server_response(HOST_ERROR error, ADDRESS_TYPE addr_type);
};
#pragma pack(pop)