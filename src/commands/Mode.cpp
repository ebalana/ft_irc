#include "../../include/Server.hpp"

void Server::_executeMode(int clientFd, const std::vector<std::string>& args)
{
	if (args.size() < 2) {
		_sendNumericReply(clientFd, "461", "MODE :Not enough parameters");
		return;
	}

	std::string channelName = args[0];
	std::string modeString = args[1];
	std::map<std::string, Channel>::iterator chanIt = _channels.find(channelName);

	if (chanIt == _channels.end())
	{
		_sendNumericReply(clientFd, "403", channelName + " :No such channel");
		return;
	}

	if (!chanIt->second.isOperator(clientFd))
	{
		_sendNumericReply(clientFd, "482", channelName + " :You're not channel operator");
		return;
	}

	if (modeString.size() < 2 || (modeString[0] != '+' && modeString[0] != '-')) { return; }

	char sign = modeString[0];
	char mode = modeString[1];
	bool active = (sign == '+');
	std::string param = (args.size() > 2) ? args[2] : "";

	if (mode == 'i') {
		chanIt->second.setInviteOnly(active);
	} 
	else if (mode == 't') {
		chanIt->second.setTopicRestricted(active);
	} 
	else if (mode == 'k') { 
		if (active && param.empty())
		{
			_sendNumericReply(clientFd, "461", "MODE +k :Needs a parameter");
			return;
		}
		chanIt->second.setKey(active ? param : "");
	} 
	else if (mode == 'l') {
		if (active && param.empty())
		{
			_sendNumericReply(clientFd, "461", "MODE +l :Needs a parameter");
			return;
		}
		chanIt->second.setUserLimit(active ? std::atoi(param.c_str()) : 0);
	} 
	else if (mode == 'o') {
		if (param.empty())
		{
			_sendNumericReply(clientFd, "461", "MODE +/-o :Needs a nickname");
			return;
		}

		int targetFd = -1;
		std::map<int, Client>::iterator clIt;
		for (clIt = _clients.begin(); clIt != _clients.end(); ++clIt)
		{
			if (clIt->second.getNickname() == param)
			{
				targetFd = clIt->first;
				break;
			}
		}

		if (targetFd == -1 || !chanIt->second.isMember(targetFd))
		{
			_sendNumericReply(clientFd, "441", param + " " + channelName + " :They aren't on that channel");
			return;
		}

		if (active) {
			chanIt->second.addOperator(&_clients.at(targetFd));
		} else {
			chanIt->second.removeOperator(targetFd);
		}
	} 
	else {
		_sendNumericReply(clientFd, "472", std::string(1, mode) + " :is unknown mode char to me");
		return;
	}

	Client& sender = _clients.at(clientFd);
	std::string modeMsg = ":" + sender.getNickname() + " MODE " + channelName + " " + modeString + (param.empty() ? "" : " " + param) + "\r\n";
	chanIt->second.broadcast(modeMsg);
}