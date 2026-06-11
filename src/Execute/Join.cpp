#include "../../include/Server.hpp"

void Server::_executeJoin(int clientFd, const std::vector<std::string>& args)
{
	if (args.empty()) {
		_sendNumericReply(clientFd, "461", "JOIN :Not enough parameters");
		return;
	}

	std::string channelName = args[0];

	if (channelName[0] != '#') {
		channelName = "#" + channelName;
	}

	Client& client = _clients.at(clientFd);
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	
	if (it != _channels.end() && it->second.isMember(clientFd)) {
		_sendNumericReply(clientFd, "443", client.getNickname() + " " + channelName + " :is already on channel");
		return; 
	}

	if (it == _channels.end()) {
		// Canal nuevo: crearlo y hacer al cliente operador
		Channel newChannel(channelName);
		_channels.insert(std::make_pair(channelName, newChannel));
		it = _channels.find(channelName);
		it->second.addMember(&client);
		it->second.addOperator(&client);
		std::cout << "Canal " << channelName << " creado por fd " << clientFd << " (Operador)." << std::endl;
	} else {
		// Canal existente: verificar restricciones ANTES de añadir
		
		// Comprobar +i (invite-only)
		if (it->second.isInviteOnly() && !it->second.isInvited(client.getNickname())) {
			_sendNumericReply(clientFd, "473", channelName + " :Cannot join channel (+i)");
			return;
		}

		// Comprobar +k (clave del canal)
		if (!it->second.getKey().empty()) {
			std::string providedKey = (args.size() > 1) ? args[1] : "";
			if (providedKey != it->second.getKey()) {
				_sendNumericReply(clientFd, "475", channelName + " :Cannot join channel (+k) - bad key");
				return;
			}
		}

		// Comprobar +l (límite de usuarios)
		if (it->second.getUserLimit() > 0 && it->second.getMemberCount() >= it->second.getUserLimit()) {
			_sendNumericReply(clientFd, "471", channelName + " :Cannot join channel (+l)");
			return;
		}

		// Si pasa todas las comprobaciones, lo añadimos
		it->second.addMember(&client);
	}

	// Notificar al canal entero que alguien ha entrado
	std::string joinMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN :" + channelName + "\r\n";
	it->second.broadcast(joinMsg);

	// Enviar el TOPIC actual de la sala al entrar
	if (it->second.getTopic().empty()) {
		_sendNumericReply(clientFd, "331", channelName + " :No topic is set");
	} else {
		_sendNumericReply(clientFd, "332", channelName + " :" + it->second.getTopic());
	}
}