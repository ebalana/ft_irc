#include "../../include/Server.hpp"

void Server::_executeNick(int clientFd, const std::vector<std::string>& args)
{
	Client& client = _clients.at(clientFd);

	if (!client.hasValidPassword()) 
	{
		_sendNumericReply(clientFd, "451", ":You have not registered (Password required)");
		return;
	}	
	if (args.empty() || args[0].empty())
	{
		_sendNumericReply(clientFd, "431", "No nickname given");
		return;
	}
	std::string newNick = args[0];
	std::map<int, Client>::iterator it;	
	for (it = _clients.begin(); it != _clients.end(); ++it) 
	{
		if (it->second.getNickname() == newNick) {
			_sendNumericReply(clientFd, "433", newNick + " :Nickname is already in use");
			return;
		}
	}
	_clients.at(clientFd).setNickname(newNick);
	std::cout << "fd " << clientFd << " cambió su nickname a: " << newNick << std::endl;

}