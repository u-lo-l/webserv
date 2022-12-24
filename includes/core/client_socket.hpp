#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include "request_message.hpp"
#include "request_parser.hpp"
#include "response_message.hpp"
#include "server_info.hpp"
#include "socket.hpp"

class ClientSocket : public Socket {
   public:
	ClientSocket(int sock_d, const std::vector<ServerInfo> &server_infos);
	~ClientSocket();
	void RecvRequest();
	void SendResponse();

	RequestMessage &GetRequestMessage();
	ResponseMessage &GetResponseMessage();

   private:
	const static int BUFFER_SIZE = 1024;
	RequestMessage request_;
	ResponseMessage response_;
	// RequestParser request_parser_;
};

#endif
