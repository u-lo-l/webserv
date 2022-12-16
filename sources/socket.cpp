#include "socket.hpp"

#include <arpa/inet.h>

Socket::Socket() {}

Socket::~Socket() {}

const int &Socket::GetType() const { return type_; }

const int &Socket::GetSocketDescriptor() const { return sock_d_; }

std::ostream &operator<<(std::ostream &out, const Socket *socket) {
	int fd = socket->GetSocketDescriptor();
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	int state = getsockname(fd, (struct sockaddr *)&addr, &addr_len);
	if (state < 0) {
		// Error
		perror("getsockname : ");
		return out;
	}
	out << "====================\n"
		<< "File Descriptor : " << fd << '\n'
		<< "host : " << inet_ntoa(addr.sin_addr) << '\n'  // 허용 함수 아님
		<< "port : " << ntohs(addr.sin_port) << '\n'
		<< "====================\n";
	return out;
}
