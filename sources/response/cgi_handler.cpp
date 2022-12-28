//
// Created by 김민준 on 2022/12/06.
//
#include "cgi_handler.hpp"

#include <unistd.h>

#include <iostream>

#include "client_socket.hpp"

CgiHandler::CgiHandler() {
}

CgiHandler::~CgiHandler() {}

/**
 * PATH=/etc/nginx
 * key : PATH
 * value : /etc/nginx
 */

void CgiHandler::ParseEnviron() {
	int i = 0;
	while (environ[i++]) {
		std::string env_var(environ[i]);
		std::size_t found = env_var.find('=');
		if (found != std::string::npos) {
			std::string key = env_var.substr(0, found);
			std::string value = env_var.substr(found + 1, env_var.length());
			cgi_envs_[key] = value;
		}
	}
}

void CgiHandler::ConvertEnvToCharSequence() {
	env_list_ = new char *[cgi_envs_.size()];  // new 실패시 예외 처리

	std::map<std::string, std::string>::const_iterator it = cgi_envs_.begin();
	size_t i = 0;
	while (i < cgi_envs_.size()) {
		std::string str = it->first + "=" + it->second;
		env_list_[i] = new char[str.length() + 1];	// new 실패시 예외 처리
		std::strcpy(env_list_[i], str.c_str());
		i++;
		it++;
	}
}

void CgiHandler::SetCgiEnvs(const RequestMessage &request, ClientSocket *client_socket) {
	RequestMessage::headers_type headers = request.GetHeaders();

	cgi_envs_["REQUEST_METHOD"] = request.GetMethod();	// METHOD
	cgi_envs_["PATH_INFO"] = request.GetResolvedUri();
	cgi_envs_["SCRIPT_NAME"] = cgi_envs_["PATH_INFO"];
	// cgi_envs_["QUERY_STRING"] = request.GetQuery();	 // QUERY
	cgi_envs_["SERVER_PROTOCOL"] = "HTTP/1.1";	// HTTP version
	// ex) multipart/form-data; boundary=---abc
	cgi_envs_["CONTENT_TYPE"] = request.GetHeaderValue("content-type");
	// request_.GetHeader("content-length");
	cgi_envs_["CONTENT_LENGTH"] = request.GetHeaderValue("content-length");

	cgi_envs_["GATEWAY_INTERFACE"] = "CGI/1.1";	 // CGI
	cgi_envs_["REMOTE_ADDR"] = client_socket->GetAddr();

	cgi_envs_["SERVER_NAME"] = request.GetHeaderValue("host");
	cgi_envs_["SERVER_PORT"] = "8080";
}

void CgiHandler::OpenPipe(KqueueHandler &kq_handler, Udata *user_data) {
	if (cgi_envs_["REQUEST_METHOD"] == "POST") {
		if (pipe(req_body_pipe_) < 0) {
			perror("pipe: ");
		}
		// user_data->type_ = PIPE_WRITE;	// PIPE_WRITE
		kq_handler.AddWriteEvent(req_body_pipe_[WRITE], user_data);
	}
	if (pipe(cgi_result_pipe_) < 0) {
		perror("pipe: ");
	}
	// user_data->type_ = PIPE_READ;  // PIPE_READ
	kq_handler.AddReadEvent(cgi_result_pipe_[READ], user_data);
}

void CgiHandler::SetupCgiResultPipe() {
	if (cgi_envs_["REQUEST_METHOD"] == "POST") {
		close(req_body_pipe_[WRITE]);
		dup2(req_body_pipe_[READ], STDIN_FILENO);
	}
	close(cgi_result_pipe_[READ]);
	dup2(cgi_result_pipe_[WRITE], STDOUT_FILENO);
}

void CgiHandler::SetupReqBodyPipe() {
	if (cgi_envs_["REQUEST_METHOD"] == "POST") {
		close(req_body_pipe_[READ]);
	}
	close(cgi_result_pipe_[WRITE]);
}

const std::string &GetCgiExecutePath(ClientSocket *client_socket) {
	return client_socket->GetServerInfo()
		.GetLocations()
		.at(client_socket->GetLocationIndex())
		.GetCgi()
		.at(1);
}

void CgiHandler::RunChildCgi(const RequestMessage &request_message) {
	SetupCgiResultPipe();

	char **argv = new char *[3];
	
	std::string php_cgi(request_message.GetCgiPath());
	argv[0] = new char[php_cgi.length() + 1];
	std::strcpy(argv[0], php_cgi.c_str());

	std::string php_file_path(cgi_envs_["PATH_INFO"]);
	argv[1] = new char[php_file_path.length() + 1];
	std::strcpy(argv[1], php_file_path.c_str());

	argv[2] = NULL;

	execve(argv[0], argv, env_list_);
	std::perror("execve : ");
	exit(0);
}

void CgiHandler::SetupAndAddEvent(KqueueHandler &kq_handler, Udata *user_data,
								  ClientSocket *client_socket) {
	RequestMessage &request_message = user_data->request_message_;
	SetCgiEnvs(request_message, client_socket);
	ConvertEnvToCharSequence();
	OpenPipe(kq_handler, user_data);

	pid_t pid = fork();
	if (pid < 0) {
		std::perror("fork: ");
		return;
	}
	std::string method = cgi_envs_["REQUEST_METHOD"];
	if (pid == 0) {
		RunChildCgi(request_message);
	} else {
		SetupReqBodyPipe();
		return;
	}
}

void CgiHandler::WriteReqBodyToCgi(
	const int &fd, const RequestMessage &request) {	 // fd is event.ident
	std::string body = request.GetBody();
	char *body_c_str = new char[body.length() + 1];
	std::strcpy(body_c_str, body.c_str());

	ssize_t result = write(fd, body_c_str, body.length() + 1);
	if (result < 0) {
		std::perror("write: ");
	}
	close(fd);
}

void CgiHandler::ReadCgiResultFormCgi(struct kevent event,
									  KqueueHandler &kq_handler) {
	Udata *user_data = reinterpret_cast<Udata *>(event.udata);
	(void)user_data;
	(void)kq_handler;
	// ClientSocket *client = reinterpret_cast<ClientSocket *>(user_data->socket_);
	// ResponseMessage &response = client->GetResponseMessage();

	// (void)response;

	char buf[1024];
	ssize_t count = read(event.ident, buf, 1024);
	if (count < 0) {
		std::perror("read: ");
		return;
	}
	if (count == 0) {
		return;
	}
	// response.AppendCgiResult(buf, count);
	if (event.data <= count) {
		close(event.ident);

		// if (response.GetCgiResult().find("Status") != std::string::npos) {
		// 	// document-response | client-redirect-response with document
		// 	// Status 302 -> client-redirect-response
		// 	// Status other -> document-response
		// } else {
		// 	// document-response (200 OK) | local-rediredct-response |
		// 	// client-redirect-response document-response -> Content-Type & body
		// 	// local-redirect-response -> local-Location header (URI path and
		// 	// query-string) client-redirect-response -> client-Location header
		// 	// (absolute URI path)
		// }
		// user_data->type_ = SEND_RESPONSE;  // SEND_RESPONE
		// kq_handler.AddWriteEvent(client->GetSocketDescriptor(), user_data);
	}
	std::map<std::string, std::string &> hi;
}
