//
// Created by 김민준 on 2022/12/21.
//

#include "udata.h"

Udata::Udata(int type) : type_(type), request_message_(-1) {}

Udata::~Udata() {}

void Udata::ChangeType(int type) { type_ = type; }

void Udata::Reset() {
	type_ = RECV_REQUEST;
	response_message_.Clear();
	request_message_.Clear();
}
