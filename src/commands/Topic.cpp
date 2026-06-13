#include "../../include/Server.hpp"

void Server::_executeTopic(int clientFd, const std::vector<std::string>& args) 
{
	if (args.empty())
	{
		_sendNumericReply(clientFd, "461", "TOPIC :Not enough parameters");
		return;
	}

	std::string channelName = args[0];
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);

	if (it == _channels.end())
	{
		_sendNumericReply(clientFd, "403", channelName + " :No such channel");
		return;
	}

	if (!it->second.isMember(clientFd)) {
		_sendNumericReply(clientFd, "442", channelName + " :You're not on that channel");
		return;
	}

	if (args.size() == 1) {
		if (it->second.getTopic().empty()) {
			_sendNumericReply(clientFd, "331", channelName + " :No topic is set");
		} else {
			_sendNumericReply(clientFd, "332", channelName + " :" + it->second.getTopic());
		}
		return;
	}

	if (it->second.isTopicRestricted() && !it->second.isOperator(clientFd))
	{
		_sendNumericReply(clientFd, "482", channelName + " :You're not channel operator");
		return;
	}
	
	std::string newTopic = args[1];
	it->second.setTopic(newTopic);
	Client& sender = _clients.at(clientFd);
	std::string topicMsg = ":" + sender.getNickname() + "!" + sender.getUsername() + "@localhost TOPIC " + channelName + " :" + newTopic + "\r\n";
	it->second.broadcast(topicMsg);
}