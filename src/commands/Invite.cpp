#include "../../include/Server.hpp"

void Server::_executeInvite(int clientFd, const std::vector<std::string>& args) {
    // 1. Verificar parámetros mínimos (Sintaxis: INVITE <usuario> <canal>)
    if (args.size() < 2) {
        _sendNumericReply(clientFd, "461", "INVITE :Not enough parameters"); // ERR_NEEDMOREPARAMS
        return;
    }

    std::string targetNick = args[0];
    std::string channelName = args[1];

    std::map<std::string, Channel>::iterator chanIt = _channels.find(channelName);

    // 2. Verificar si el canal existe
    if (chanIt == _channels.end()) {
        _sendNumericReply(clientFd, "403", channelName + " :No such channel"); // ERR_NOSUCHCHANNEL
        return;
    }

    // 3. Verificar si el ejecutor está en el canal
    if (!chanIt->second.isMember(clientFd)) {
        _sendNumericReply(clientFd, "442", channelName + " :You're not on that channel"); // ERR_NOTONCHANNEL
        return;
    }

    // 4. Si el canal es Invite-Only (+i), exigir que el ejecutor sea Operador
    if (chanIt->second.isInviteOnly() && !chanIt->second.isOperator(clientFd)) {
        _sendNumericReply(clientFd, "482", channelName + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }

    // 5. Buscar el FD del usuario al que queremos invitar
    int targetFd = -1;
    std::map<int, Client>::iterator clIt;
    for (clIt = _clients.begin(); clIt != _clients.end(); ++clIt) {
        if (clIt->second.getNickname() == targetNick) {
            targetFd = clIt->first;
            break;
        }
    }

    // Verificar si el usuario invitado existe conectado en el servidor
    if (targetFd == -1) {
        _sendNumericReply(clientFd, "401", targetNick + " :No such nick/channel"); // ERR_NOSUCHNICK
        return;
    }

    // 6. Verificar si el invitado ya se encuentra dentro del canal
    if (chanIt->second.isMember(targetFd)) {
        _sendNumericReply(clientFd, "443", targetNick + " " + channelName + " :is already on channel"); // ERR_USERONCHANNEL
        return;
    }

    // 7. Registrar la invitación de manera oficial en el canal
    chanIt->second.addInvitation(targetNick);

    // 8. Enviar respuestas correspondientes (RPL_INVITING = 341 al emisor, y la alerta al invitado)
    _sendNumericReply(clientFd, "341", targetNick + " " + channelName);

    Client& sender = _clients.at(clientFd);
    std::string inviteMsg = ":" + sender.getNickname() + "!" + sender.getUsername() 
                        + "@localhost INVITE " + targetNick + " :" + channelName + "\r\n";
    send(targetFd, inviteMsg.c_str(), inviteMsg.length(), 0);

    std::cout << sender.getNickname() << " invitó a " << targetNick << " a entrar a " << channelName << std::endl;
}