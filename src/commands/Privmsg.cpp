#include "../../include/Server.hpp"

void Server::_executePrivmsg(int clientFd, const std::vector<std::string>& args) 
{
	if (args.empty()) {
		_sendNumericReply(clientFd, "411", "No recipient given (PRIVMSG)");
		return;
	}
	if (args.size() < 2 || args[1].empty()) 
	{
		_sendNumericReply(clientFd, "412", "No text to send");
		return;
	}

	std::string target = args[0];
	std::string message = args[1];
	Client& sender = _clients.at(clientFd);

	std::string formattedMsg = ":" + sender.getNickname() + "!" + sender.getUsername() + "@localhost PRIVMSG " + target + " :" + message + "\r\n";

	if (target[0] == '#') 
	{
		std::map<std::string, Channel>::iterator it = _channels.find(target);
		if (it == _channels.end())
		{
			_sendNumericReply(clientFd, "403", target + " :No such channel");
			return;
		}
		if (!it->second.isMember(clientFd))
		{
			_sendNumericReply(clientFd, "442", target + " :You're not on that channel");
			return;
		}
		it->second.broadcast(formattedMsg, clientFd);
	}
	else {
		int targetFd = -1;
		std::map<int, Client>::iterator it;
		
		for (it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second.getNickname() == target) {
				targetFd = it->first;
				break;
			}
		}

		if (targetFd == -1)
		{
			_sendNumericReply(clientFd, "401", target + " :No such nick/channel");
			return;
		}

		send(targetFd, formattedMsg.c_str(), formattedMsg.length(), 0);
	}
}