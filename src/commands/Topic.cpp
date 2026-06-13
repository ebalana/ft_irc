#include "../../include/Server.hpp"

void Server::_executeTopic(int clientFd, const std::vector<std::string>& args) {
    if (args.empty()) {
        _sendNumericReply(clientFd, "461", "TOPIC :Not enough parameters"); // ERR_NEEDMOREPARAMS
        return;
    }

    std::string channelName = args[0];
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    
    // Verificar si el canal existe
    if (it == _channels.end()) {
        _sendNumericReply(clientFd, "403", channelName + " :No such channel"); // ERR_NOSUCHCHANNEL
        return;
    }

    // Verificar si el usuario que lo solicita pertenece al canal
    if (!it->second.isMember(clientFd)) {
        _sendNumericReply(clientFd, "442", channelName + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }

    // CASO A: Solo consultar el TOPIC (ej: TOPIC #sala)
    if (args.size() == 1) {
        if (it->second.getTopic().empty()) {
            _sendNumericReply(clientFd, "331", channelName + " :No topic is set"); // RPL_NOTOPIC
        } else {
            _sendNumericReply(clientFd, "332", channelName + " :" + it->second.getTopic()); // RPL_TOPIC
        }
        return;
    }

    // CASO B: Intentar cambiar el TOPIC (ej: TOPIC #sala :nuevo tema)
    // Requisito del subject: comprobar las restricciones del modo 't'
    if (it->second.isTopicRestricted() && !it->second.isOperator(clientFd)) {
        _sendNumericReply(clientFd, "482", channelName + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }

    // Cambiar el tema en el objeto canal
    std::string newTopic = args[1];
    it->second.setTopic(newTopic);

    // Notificar a TODOS los miembros del canal que el topic ha cambiado (Formato IRC oficial)
    // Formato: :<nick>!<user>@localhost TOPIC <canal> :<nuevo_topic>\r\n
    Client& sender = _clients.at(clientFd);
    std::string topicMsg = ":" + sender.getNickname() + "!" + sender.getUsername() + "@localhost TOPIC " + channelName + " :" + newTopic + "\r\n";
    it->second.broadcast(topicMsg);
}