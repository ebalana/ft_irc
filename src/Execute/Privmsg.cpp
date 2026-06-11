#include "../../include/Server.hpp"

void Server::_executePrivmsg(int clientFd, const std::vector<std::string>& args) {
    // 1. Validar que tengamos el receptor y el mensaje
    if (args.empty()) {
        _sendNumericReply(clientFd, "411", "No recipient given (PRIVMSG)"); // ERR_NORECIPIENT
        return;
    }
    if (args.size() < 2 || args[1].empty()) {
        _sendNumericReply(clientFd, "412", "No text to send"); // ERR_NOTEXTTOSEND
        return;
    }

    std::string target = args[0];
    std::string message = args[1];
    Client& sender = _clients.at(clientFd);

    // Formato estándar de un mensaje IRC enviado por la red:
    // :<nick_emisor>!<user_emisor>@localhost PRIVMSG <receptor> :<mensaje>\r\n
    std::string formattedMsg = ":" + sender.getNickname() + "!" + sender.getUsername() + "@localhost PRIVMSG " + target + " :" + message + "\r\n";

    // CASO A: El receptor es un CANAL
    if (target[0] == '#') {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it == _channels.end()) {
            _sendNumericReply(clientFd, "403", target + " :No such channel"); // ERR_NOSUCHCHANNEL
            return;
        }

        // Verificar si el emisor es miembro del canal (buena práctica en IRC)
        if (!it->second.isMember(clientFd)) {
            _sendNumericReply(clientFd, "442", target + " :You're not on that channel"); // ERR_NOTONCHANNEL
            return;
        }

        // Reenviar a todos los miembros del canal EXCEPTO al emisor 
        it->second.broadcast(formattedMsg, clientFd);
    }
    // CASO B: El receptor es un USUARIO PRIVADO
    else {
        int targetFd = -1;
        std::map<int, Client>::iterator it;
        
        // Buscar al cliente por su nickname
        for (it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.getNickname() == target) {
                targetFd = it->first;
                break;
            }
        }

        if (targetFd == -1) {
            _sendNumericReply(clientFd, "401", target + " :No such nick/channel"); // ERR_NOSUCHNICK
            return;
        }

        // Enviar el mensaje privado directamente a su descriptor 
        send(targetFd, formattedMsg.c_str(), formattedMsg.length(), 0);
    }
}