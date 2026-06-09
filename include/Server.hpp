#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Channel.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h> // Funciones de sockets
#include <netinet/in.h> // sockaddr_in
#include <poll.h>       // pollfd
#include <fcntl.h>      // fcntl
#include <unistd.h>     // close
#include <stdexcept>
#include <cstdlib> // Para std::atoi
#include <cstring>   // Para std::memset
#include <arpa/inet.h> // Para inet_ntoa
#include <map>

class Server 
{
	private:
		int _port;
		std::string _password;
		int _serverSocketFd;
		
		// Vector para guardar los file descriptors que poll() va a monitorizar
		std::vector<struct pollfd> _pollFds;

		// Deshabilitar constructor copia y asignación (Ortodoxa)
		Server(const Server& src);
		Server& operator=(const Server& src);

		void _handleNewConnection();
		void _handleClientData(size_t index);

		std::map<int, Client> _clients;
		std::map<std::string, Channel> _channels;

		void _processCommand(int clientFd, const std::string& command);
		void _executePass(int clientFd, const std::vector<std::string>& args);
		void _executeNick(int clientFd, const std::vector<std::string>& args);
		void _executeUser(int clientFd, const std::vector<std::string>& args);
		void _sendNumericReply(int clientFd, const std::string& numeric, const std::string& message);

		void _executeJoin(int clientFd, const std::vector<std::string>& args);
		void _executePrivmsg(int clientFd, const std::vector<std::string>& args);
		void _executeTopic(int clientFd, const std::vector<std::string>& args);
		void _executeKick(int clientFd, const std::vector<std::string>& args);
		void _executeInvite(int clientFd, const std::vector<std::string>& args);
		void _executeMode(int clientFd, const std::vector<std::string>& args);

		void _leaveAllChannels(int clientFd);

	public:
		Server(int port, const std::string& password);
		~Server();

		void init(); // Configura el socket principal
		void run();  // Arranca el bucle de eventos (poll)
};

#endif