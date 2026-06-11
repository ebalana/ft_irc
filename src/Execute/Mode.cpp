#include "../../include/Server.hpp"

void Server::_executeMode(int clientFd, const std::vector<std::string>& args) {
    // 1. Validar parámetros mínimos (Sintaxis básica: MODE <canal> <+/-modo>)
    if (args.size() < 2) {
        _sendNumericReply(clientFd, "461", "MODE :Not enough parameters");
        return;
    }

    std::string channelName = args[0];
    std::string modeString = args[1];

    std::map<std::string, Channel>::iterator chanIt = _channels.find(channelName);
    
    // 2. Verificar si el canal existe en el servidor
    if (chanIt == _channels.end()) {
        _sendNumericReply(clientFd, "403", channelName + " :No such channel");
        return;
    }

    // 3. REQUISITO: Verificar si el ejecutor del comando es operador del canal
    if (!chanIt->second.isOperator(clientFd)) {
        _sendNumericReply(clientFd, "482", channelName + " :You're not channel operator");
        return;
    }

    // Validar el formato mínimo de la cadena de modo (ej: "+i" o "-t")
    if (modeString.size() < 2 || (modeString[0] != '+' && modeString[0] != '-')) {
        return; // Formato incorrecto, salimos de forma segura
    }

    char sign = modeString[0];
    char mode = modeString[1];
    bool active = (sign == '+');

    // Capturar el parámetro extra si existe (ej: la clave, el límite o el nick)
    std::string param = (args.size() > 2) ? args[2] : "";

    // 4. Procesamiento de Banderas / Modos Obligatorios del Subject
    if (mode == 'i') { 
        // Modo i: Set/remove Invite-only channel
        chanIt->second.setInviteOnly(active);
    } 
    else if (mode == 't') { 
        // Modo t: Restricción del comando TOPIC a operadores
        chanIt->second.setTopicRestricted(active);
    } 
    else if (mode == 'k') { 
        // Modo k: Set/remove the channel key (password)
        if (active && param.empty()) {
            _sendNumericReply(clientFd, "461", "MODE +k :Needs a parameter");
            return;
        }
        chanIt->second.setKey(active ? param : "");
    } 
    else if (mode == 'l') { 
        // Modo l: Set/remove the user limit to channel
        if (active && param.empty()) {
            _sendNumericReply(clientFd, "461", "MODE +l :Needs a parameter");
            return;
        }
        chanIt->second.setUserLimit(active ? std::atoi(param.c_str()) : 0);
    } 
    else if (mode == 'o') { 
        // Modo o: Give/take channel operator privilege
        if (param.empty()) {
            _sendNumericReply(clientFd, "461", "MODE +/-o :Needs a nickname");
            return;
        }
        
        // Buscar el File Descriptor del usuario objetivo por su apodo
        int targetFd = -1;
        std::map<int, Client>::iterator clIt;
        for (clIt = _clients.begin(); clIt != _clients.end(); ++clIt) {
            if (clIt->second.getNickname() == param) {
                targetFd = clIt->first;
                break;
            }
        }

        // Verificar que el usuario exista y pertenezca a este canal
        if (targetFd == -1 || !chanIt->second.isMember(targetFd)) {
            _sendNumericReply(clientFd, "441", param + " " + channelName + " :They aren't on that channel");
            return;
        }

        // Otorgar o quitar el rol de operador en el canal
        if (active) {
            chanIt->second.addOperator(&_clients.at(targetFd));
        } else {
            chanIt->second.removeOperator(targetFd);
        }
    } 
    else {
        // Error estándar si el cliente manda un modo extraño
        _sendNumericReply(clientFd, "472", std::string(1, mode) + " :is unknown mode char to me");
        return;
    }

    // 5. Notificar el cambio de modo de manera oficial a todo el canal
    // Formato: :<nick_ejecutor> MODE <canal> <+/-modo> [parámetro]\r\n
    Client& sender = _clients.at(clientFd);
    std::string modeMsg = ":" + sender.getNickname() + " MODE " + channelName + " " + modeString + (param.empty() ? "" : " " + param) + "\r\n";
    chanIt->second.broadcast(modeMsg);
}