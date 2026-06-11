#include "../include/Client.hpp"

Client::Client(int fd) : _fd(fd), _isRegistered(false) {
    _nickname = "";
    _username = "";
    _buffer = "";
}

Client::~Client() {}

int Client::getFd() const { return _fd; }

std::string Client::getNickname() const { return _nickname; }
void Client::setNickname(const std::string& nick) { _nickname = nick; }

std::string Client::getUsername() const { return _username; }
void Client::setUsername(const std::string& user) { _username = user; }

std::string& Client::getBuffer() { return _buffer; }
void Client::appendToBuffer(const std::string& str) { _buffer += str; }
void Client::clearBuffer() { _buffer.clear(); }

bool Client::isRegistered() const { return _isRegistered; }
void Client::setRegistered(bool value) { _isRegistered = value; }

bool Client::hasValidPassword() const { return _hasValidPassword; }
void Client::setValidPassword(bool value) { _hasValidPassword = value; }
