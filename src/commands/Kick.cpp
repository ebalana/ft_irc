#include "../../include/Server.hpp"

void Server::_executeKick(int clientFd, const std::vector<std::string>& args) {
    // 1. Verificar parámetros mínimos (Canal y Usuario)
    if (args.size() < 2) {
        _sendNumericReply(clientFd, "461", "KICK :Not enough parameters"); // ERR_NEEDMOREPARAMS
        return;
    }

    std::string channelName = args[0];
    std::string targetNick = args[1];
    
    // Si pasaron una razón de expulsión (ej: :Spam), la usamos; si no, ponemos una por defecto
    std::string reason = (args.size() > 2) ? args[2] : "No reason specified";

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

    // 4. REQUISITO: ¿Es el ejecutor un operador del canal?
    if (!chanIt->second.isOperator(clientFd)) {
        _sendNumericReply(clientFd, "482", channelName + " :You're not channel operator"); // ERR_CHANOPRIVSNEEDED
        return;
    }

    // 5. Buscar el FD del usuario objetivo (targetNick)
    int targetFd = -1;
    std::map<int, Client>::iterator clIt;
    for (clIt = _clients.begin(); clIt != _clients.end(); ++clIt) {
        if (clIt->second.getNickname() == targetNick) {
            targetFd = clIt->first;
            break;
        }
    }

    // 6. Verificar si el usuario objetivo existe en el servidor y está en el canal
    if (targetFd == -1 || !chanIt->second.isMember(targetFd)) {
        _sendNumericReply(clientFd, "441", targetNick + " " + channelName + " :They aren't on that channel"); // ERR_USERNOTINCHANNEL
        return;
    }

    // 7. Construir y enviar el mensaje oficial de expulsión a todo el canal
    // Formato: :<operador> KICK <canal> <expulsado> :<razón>\r\n
    Client& operatorUser = _clients.at(clientFd);
    std::string kickMsg = ":" + operatorUser.getNickname() + "!" + operatorUser.getUsername() 
                        + "@localhost KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
    
    // Avisamos a todos los miembros actuales (incluyendo al que va a ser expulsado)
    chanIt->second.broadcast(kickMsg);

    // 8. Expulsar efectivamente al usuario del objeto canal
    chanIt->second.removeMember(targetFd);
    std::cout << "Usuario " << targetNick << " fue expulsado de " << channelName << " por " << operatorUser.getNickname() << std::endl;
}