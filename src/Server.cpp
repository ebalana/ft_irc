#include "../include/Server.hpp"

Server::Server(int port, const std::string& password) 
	: _port(port), _password(password), _serverSocketFd(-1) {}

Server::~Server()
{
	if (_serverSocketFd != -1) {
		close(_serverSocketFd);
		std::cout << "Servidor cerrado correctamente." << std::endl;
	}
}

void Server::init() 
{
	_serverSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocketFd == -1) {
		throw std::runtime_error("Error al crear el socket del servidor.");
	}

	int opt = 1;
	if (setsockopt(_serverSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en setsockopt().");
	}

	if (fcntl(_serverSocketFd, F_SETFL, O_NONBLOCK) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error al configurar fcntl() en modo no bloqueante.");
	}

	struct sockaddr_in serverAddr;
	std::memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(_port);

	if (bind(_serverSocketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en bind(): El puerto ya podría estar en uso.");
	}

	if (listen(_serverSocketFd, SOMAXCONN) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en listen().");
	}

	struct pollfd serverPollFd;
	serverPollFd.fd = _serverSocketFd;
	serverPollFd.events = POLLIN;
	serverPollFd.revents = 0;
	_pollFds.push_back(serverPollFd);

	std::cout << "Servidor escuchando en el puerto " << _port << "..." << std::endl;
}

void Server::run()
{
	std::cout << "Servidor en marcha. Esperando eventos..." << std::endl;

	while (true) {
		int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);
		
		if (pollCount == -1) {
			throw std::runtime_error("Error en la llamada a poll().");
		}

		for (int i = (int)_pollFds.size() - 1; i >= 0; i--) 
		{
			
			if (_pollFds[i].revents == 0)
				continue;
			if (_pollFds[i].fd == _serverSocketFd) {
				if (_pollFds[i].revents & POLLIN) {
					_handleNewConnection();
				}
				continue;
			} 
			if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				int clientFd = _pollFds[i].fd;
				std::cout << "Desconexión detectada por poll() en fd: " << clientFd << std::endl;
				
				_leaveAllChannels(clientFd);
				close(clientFd);
				_clients.erase(clientFd);
				_pollFds.erase(_pollFds.begin() + i);
				continue;
			}
			if (_pollFds[i].revents & POLLIN) {
				_handleClientData(i);
			}
		}
	}
}

void Server::_handleNewConnection()
{
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);

	int clientFd = accept(_serverSocketFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
	if (clientFd == -1) {
		std::cerr << "Error al aceptar una nueva conexión." << std::endl;
		return;
	}

	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
		std::cerr << "Error en fcntl() con el nuevo cliente." << std::endl;
		close(clientFd);
		return;
	}

	struct pollfd clientPollFd;
	clientPollFd.fd = clientFd;
	clientPollFd.events = POLLIN;
	clientPollFd.revents = 0;
	_pollFds.push_back(clientPollFd);

	_clients.insert(std::make_pair(clientFd, Client(clientFd)));

	std::cout << "Nueva conexión aceptada desde la IP: " 
				<< inet_ntoa(clientAddr.sin_addr) 
				<< " en el fd: " << clientFd << std::endl;
}

void Server::_handleClientData(size_t index)
{
	int clientFd = _pollFds[index].fd;
	char buffer[1024];
	std::memset(buffer, 0, sizeof(buffer));
	int bytesReceived = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived <= 0) 
	{
		if (bytesReceived < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;
		if (bytesReceived == 0)
			std::cout << "Cliente en fd " << clientFd << " se ha desconectado." << std::endl;
		else
			std::cerr << "Error en recv() con el fd " << clientFd << std::endl;
		_leaveAllChannels(clientFd);
		close(clientFd);
		_pollFds.erase(_pollFds.begin() + index);
		_clients.erase(clientFd);
		return;
	}

	_clients.at(clientFd).appendToBuffer(std::string(buffer, bytesReceived));
	std::string& clientBuffer = _clients.at(clientFd).getBuffer();
	size_t pos;

	while ((pos = clientBuffer.find('\n')) != std::string::npos) 
	{
		
		std::string command = clientBuffer.substr(0, pos);
		
		if (!command.empty() && command[command.size() - 1] == '\r') {
			command.erase(command.size() - 1);
		}
		clientBuffer.erase(0, pos + 1);
		if (!command.empty()) {
			std::cout << "Ejecutando desde fd " << clientFd << ": [" << command << "]" << std::endl;
			_processCommand(clientFd, command);
		}
	}
}

void Server::_leaveAllChannels(int clientFd) 
{
	std::map<std::string, Channel>::iterator it;
	for (it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->second.isMember(clientFd)) 
		{
			it->second.removeMember(clientFd);
		}
	}
}

void Server::_sendNumericReply(int clientFd, const std::string& numeric, const std::string& message)
{
	std::string nick = _clients.at(clientFd).getNickname();
	if (nick.empty()) nick = "*";
	std::string reply = ":localhost " + numeric + " " + nick + " :" + message + "\r\n";
	send(clientFd, reply.c_str(), reply.length(), 0);
}

void Server::_processCommand(int clientFd, const std::string& commandLine) 
{
	if (commandLine.empty()) return;

	std::stringstream ss(commandLine);
	std::string token;
	std::vector<std::string> args;
	
	ss >> token;	
	for (size_t i = 0; i < token.length(); i++) {
		token[i] = std::toupper(token[i]);
	}
	std::string cmd = token;

	while (ss >> token) 
	{
		if (token[0] == ':')
		{
			std::string trailing;
			std::getline(ss, trailing);
			args.push_back(token.substr(1) + trailing);
			break;
		}
		args.push_back(token);
	}

	std::cout << "Ejecutando comando [" << cmd << "] con " << args.size() << " argumentos." << std::endl;

	if (cmd == "PASS") {
		_executePass(clientFd, args);
	} 
	else if (cmd == "NICK") {
		_executeNick(clientFd, args);
	} 
	else if (cmd == "USER") {
		_executeUser(clientFd, args);
	}
	else if (cmd == "JOIN") 
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeJoin(clientFd, args);
		}
	}
	else if (cmd == "PRIVMSG") 
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executePrivmsg(clientFd, args);
		}
	}
	else if (cmd == "TOPIC")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeTopic(clientFd, args);
		}
	}
	else if (cmd == "KICK")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeKick(clientFd, args);
		}
	}
	else if (cmd == "INVITE")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeInvite(clientFd, args);
		}
	}
	else if (cmd == "MODE")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeMode(clientFd, args);
		}
	}
	else {
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_sendNumericReply(clientFd, "421", cmd + " :Unknown command");
		}
	}
}