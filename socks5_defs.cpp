#include "socks5_defs.hpp"

server_hello::server_hello() : socks_ver(5), auth_method(0) {}

server_response::server_response(HOST_ERROR error, ADDRESS_TYPE addr_type) : socks_ver(5), error(error), reserved(0), addr_type(addr_type) {}