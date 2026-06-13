#include "../../include/Server.hpp"

void Server::_executeUser(int clientFd, const std::vector<std::string>& args) 
{
	Client& client = _clients.at(clientFd);
	
	if (!client.hasValidPassword()) {
		_sendNumericReply(clientFd, "451", ":You have not registered (Password required)"); // ERR_NOTREGISTERED
		return;
	}

	if (client.isRegistered()) {
		_sendNumericReply(clientFd, "462", "You may not reregister"); // ERR_ALREADYREGISTRED
		return;
	}

	if (args.size() < 4) {
		_sendNumericReply(clientFd, "461", "USER :Not enough parameters"); // ERR_NEEDMOREPARAMS
		return;
	}

	// El primer argumento es el username
	client.setUsername(args[0]);
	std::cout << "fd " << clientFd << " configuró su username a: " << args[0] << std::endl;

	// Aquí ya podemos verificar si el cliente cumple las condiciones para ser bienvenido
	if (!client.getNickname().empty() && !client.getUsername().empty()) {
		client.setRegistered(true);
		// Enviamos el mensaje de bienvenida oficial del protocolo IRC (RPL_WELCOME)
		_sendNumericReply(clientFd, "001", "Welcome to the Internet Relay Network " + client.getNickname());
		std::cout << "Cliente fd " << clientFd << " se ha REGISTRADO con éxito." << std::endl;
	}
}