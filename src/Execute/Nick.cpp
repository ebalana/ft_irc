#include "../../include/Server.hpp"

void Server::_executeNick(int clientFd, const std::vector<std::string>& args)
{
	Client& client = _clients.at(clientFd);

	if (!client.hasValidPassword()) {
		_sendNumericReply(clientFd, "451", ":You have not registered (Password required)"); // ERR_NOTREGISTERED
		return;
	}
	
	if (args.empty() || args[0].empty()) {
		_sendNumericReply(clientFd, "431", "No nickname given"); // ERR_NONICKNAMEGIVEN
		return;
	}

	std::string newNick = args[0];

	// Verificar si el nickname ya está en uso por otro cliente
	std::map<int, Client>::iterator it;
	for (it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second.getNickname() == newNick) {
			_sendNumericReply(clientFd, "433", newNick + " :Nickname is already in use"); // ERR_NICKNAMEINUSE
			return;
		}
	}

	// Asignar el nuevo nickname
	_clients.at(clientFd).setNickname(newNick);
	std::cout << "fd " << clientFd << " cambió su nickname a: " << newNick << std::endl;

	// Si ya tiene USER y PASS correctos, intentamos registrarlo definitivamente
	// (Crearemos una función auxiliar para verificar esto en un momento)
}