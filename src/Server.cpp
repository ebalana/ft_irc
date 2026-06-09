#include "../include/Server.hpp"

Server::Server(int port, const std::string& password) 
	: _port(port), _password(password), _serverSocketFd(-1) {}

Server::~Server()
{
	// Cerramos el socket del servidor al destruir la clase
	if (_serverSocketFd != -1) {
		close(_serverSocketFd);
		std::cout << "Servidor cerrado correctamente." << std::endl;
	}

	// También cerraremos los sockets de los clientes aquí más adelante
}

void Server::init() 
{
	// 1. Crear el socket del servidor
	// AF_INET: IPv4 | SOCK_STREAM: TCP | 0: Protocolo por defecto (TCP)
	_serverSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocketFd == -1) {
		throw std::runtime_error("Error al crear el socket del servidor.");
	}

	// 2. Configurar SO_REUSEADDR para permitir reutilizar el puerto inmediatamente
	int opt = 1;
	if (setsockopt(_serverSocketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en setsockopt().");
	}

	// 3. Configurar el socket en modo NO BLOQUEANTE (Requisito estricto del subject)
	// En MacOS solo se permite el flag O_NONBLOCK de esta forma exacta
	if (fcntl(_serverSocketFd, F_SETFL, O_NONBLOCK) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error al configurar fcntl() en modo no bloqueante.");
	}

	// 4. Preparar la estructura de la dirección del servidor (IP y Puerto)
	struct sockaddr_in serverAddr;
	std::memset(&serverAddr, 0, sizeof(serverAddr)); // Limpiar la estructura
	serverAddr.sin_family = AF_INET;                 // IPv4
	serverAddr.sin_addr.s_addr = INADDR_ANY;         // Escuchar en cualquier interfaz de red (0.0.0.0)
	serverAddr.sin_port = htons(_port);              // Convertir el puerto a "Network Byte Order" (Big Endian)

	// 5. Vincular el socket a la dirección y puerto (bind)
	if (bind(_serverSocketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en bind(): El puerto ya podría estar en uso.");
	}

	// 6. Poner el socket en modo escucha (listen)
	// SOMAXCONN es el máximo de conexiones en cola que el sistema operativo permite
	if (listen(_serverSocketFd, SOMAXCONN) == -1) {
		close(_serverSocketFd);
		throw std::runtime_error("Error en listen().");
	}

	// 7. Registrar el socket del servidor en nuestro vector de pollfd
	// Queremos que poll() nos avise cuando haya una nueva conexión entrante (POLLIN)
	struct pollfd serverPollFd;
	serverPollFd.fd = _serverSocketFd;
	serverPollFd.events = POLLIN;
	serverPollFd.revents = 0;
	_pollFds.push_back(serverPollFd);

	std::cout << "Servidor escuchando en el puerto " << _port << "..." << std::endl;
}

void Server::run()
{
	std::cout << "Servidor en marcha. Esperando eventos..." << std::endl;

	// Bucle principal del servidor (infinito, hasta que se detenga el proceso)
	while (true) {
		// poll() recibe: el puntero al array, el número de elementos y el timeout (-1 significa esperar indefinidamente)
		int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);
		
		if (pollCount == -1) {
			throw std::runtime_error("Error en la llamada a poll().");
		}

		// Recorremos todos los file descriptors que poll() está vigilando
		for (size_t i = 0; i < _pollFds.size(); i++) {
			
			// Si no ha ocurrido ningún evento en este fd, pasamos al siguiente
			if (_pollFds[i].revents == 0)
				continue;

			// CASO 1: Actividad en el socket del servidor -> NUEVA CONEXIÓN
			if (_pollFds[i].fd == _serverSocketFd) {
				if (_pollFds[i].revents & POLLIN) {
					_handleNewConnection();
				}
			} 
			// CASO 2: Actividad en el socket de un cliente -> MENSAJE ENTRANTE O DESCONEXIÓN
			else {
				if (_pollFds[i].revents & POLLIN) {
					_handleClientData(i);
				}
			}
		}
	}
}

void Server::_handleNewConnection()
{
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);

	// Aceptamos la conexión entrante
	int clientFd = accept(_serverSocketFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
	if (clientFd == -1) {
		std::cerr << "Error al aceptar una nueva conexión." << std::endl;
		return;
	}

	// Configurar el socket del cliente en modo NO BLOQUEANTE (Requisito estricto del subject)
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
		std::cerr << "Error en fcntl() con el nuevo cliente." << std::endl;
		close(clientFd);
		return;
	}

	// Añadir el nuevo cliente al vector de poll para que sea vigilado en el próximo ciclo
	struct pollfd clientPollFd;
	clientPollFd.fd = clientFd;
	clientPollFd.events = POLLIN; // Nos interesa saber cuándo envía datos
	clientPollFd.revents = 0;
	_pollFds.push_back(clientPollFd);

	_clients.insert(std::make_pair(clientFd, Client(clientFd)));

	std::cout << "Nueva conexión aceptada desde la IP: " 
				<< inet_ntoa(clientAddr.sin_addr) 
				<< " en el fd: " << clientFd << std::endl;
}

void Server::_handleClientData(size_t index)
{
	char buffer[1024];
	int clientFd = _pollFds[index].fd;

	std::memset(buffer, 0, sizeof(buffer));
	int bytesReceived = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

	if (bytesReceived <= 0) {
		if (bytesReceived == 0) {
			std::cout << "Cliente en fd " << clientFd << " se ha desconectado." << std::endl;
		} else {
			std::cerr << "Error en recv() con el fd " << clientFd << std::endl;
		}

		// Limpiar al usuario de todos los canales antes de destruirlo ---
		_leaveAllChannels(clientFd);

		close(clientFd);
		_pollFds.erase(_pollFds.begin() + index);
		_clients.erase(clientFd); // --- NUEVO: Eliminar al cliente del mapa ---
	} 
    else {
		// 1. Añadir los bytes recibidos al buffer específico de este cliente
		_clients.at(clientFd).appendToBuffer(std::string(buffer));

		// 2. Procesar el buffer para extraer comandos completos
		std::string& clientBuffer = _clients.at(clientFd).getBuffer();
		size_t pos;

		// Buscamos si hay un salto de línea en el buffer acumulado
		while ((pos = clientBuffer.find("\n")) != std::string::npos) {
			// Extraemos el comando hasta el '\n'
			std::string command = clientBuffer.substr(0, pos);
			
			// Limpiamos también el '\r' si el cliente (como Netcat -C o HexChat) lo envía (\r\n)
			if (!command.empty() && command[command.size() - 1] == '\r') {
				command.erase(command.size() - 1);
			}

			// Si el comando no está vacío, lo ejecutamos
			if (!command.empty()) {
				std::cout << "Comando completo reconstruido desde fd " << clientFd << ": [" << command << "]" << std::endl;
				
				// Aquí llamaremos a nuestro futuro parseador de comandos de IRC
				// Por ahora, devolvemos un eco del comando limpio
				// std::string response = "Procesando comando: " + command + "\r\n";
				// send(clientFd, response.c_str(), response.length(), 0);
				if (!command.empty()) {
					_processCommand(clientFd, command);
				}
			}

			// Eliminamos el comando procesado del buffer del cliente
			clientBuffer.erase(0, pos + 1);
		}
	}
}

void Server::_leaveAllChannels(int clientFd) {
    std::map<std::string, Channel>::iterator it;
    for (it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second.isMember(clientFd)) {
            it->second.removeMember(clientFd);            
            // Opcional: Si el canal se queda completamente vacío (0 miembros), 
            // es una buena práctica en IRC borrar el canal del mapa para no consumir memoria.
        }
    }
}

void Server::_executePass(int clientFd, const std::vector<std::string>& args)
{
	Client& client = _clients.at(clientFd);

	// Si ya está registrado, el protocolo dice que no se puede volver a mandar PASS
	if (client.isRegistered()) {
		_sendNumericReply(clientFd, "462", "You may not reregister"); // ERR_ALREADYREGISTRED
		return;
	}

	// Comprobar si pasaron el argumento de la contraseña
	if (args.empty()) {
		_sendNumericReply(clientFd, "461", "PASS :Not enough parameters"); // ERR_NEEDMOREPARAMS
		return;
	}

	// Validar la contraseña enviada contra la del servidor
	if (args[0] != _password) {
		_sendNumericReply(clientFd, "464", "Password incorrect"); // ERR_PASSWDMISMATCH
		std::cout << "fd " << clientFd << " falló la autenticación (Contraseña incorrecta)." << std::endl;
		
		// Opcional pero recomendado: desconectar al cliente si falla la contraseña
		// Para este paso, solo dejamos que falle.
		return;
	}

	// Si la contraseña es correcta, guardamos un estado interno o simplemente dejamos que prosiga.
	// Ojo: No lo marcamos como 'isRegistered = true' todavía, porque le falta NICK y USER.
	std::cout << "fd " << clientFd << " superó el chequeo de contraseña con éxito." << std::endl;
}

// ## Implementación de _processCommand

// Función auxiliar para enviar respuestas numéricas formateadas estilo IRC
void Server::_sendNumericReply(int clientFd, const std::string& numeric, const std::string& message)
{
	// Formato estándar: :localhost <número> <nickname_o_*> :<mensaje>\r\n
	std::string nick = _clients.at(clientFd).getNickname();
	if (nick.empty()) nick = "*"; // Si aún no tiene nick, se usa '*'

	std::string reply = ":localhost " + numeric + " " + nick + " :" + message + "\r\n";
	send(clientFd, reply.c_str(), reply.length(), 0);
}

void Server::_processCommand(int clientFd, const std::string& commandLine) 
{
	if (commandLine.empty()) return;

	std::stringstream ss(commandLine);
	std::string token;
	std::vector<std::string> args;

	// 1. Extraer el comando principal (ej. PASS, NICK...)
	ss >> token;

	// Convertir el comando a mayúsculas para que sea insensible a mayúsculas/minúsculas (ej. "pass" -> "PASS")
	for (size_t i = 0; i < token.length(); i++) {
		token[i] = std::toupper(token[i]);
	}

	std::string cmd = token;

	// 2. Extraer los argumentos
	while (ss >> token) {
		// Si un argumento empieza por ':', significa que es el "trailing" (el resto de la línea va junta)
		if (token[0] == ':') {
			std::string trailing;
			std::getline(ss, trailing);
			// El getline se queda con lo que sobra, le sumamos la palabra actual quitándole los ':'
			args.push_back(token.substr(1) + trailing);
			break;
		}
		args.push_back(token);
	}

	std::cout << "Ejecutando comando [" << cmd << "] con " << args.size() << " argumentos." << std::endl;

	// 3. Enrutador de comandos (Comprobamos la fase de autenticación)
	if (cmd == "PASS") {
		_executePass(clientFd, args);
	} 
	else if (cmd == "NICK") {
		_executeNick(clientFd, args);
	} 
	else if (cmd == "USER") {
		_executeUser(clientFd, args);
	}
	else if (cmd == "JOIN") 
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeJoin(clientFd, args);
		}
	}
	else if (cmd == "PRIVMSG") 
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered"); // ERR_NOTREGISTERED
		} else {
			_executePrivmsg(clientFd, args);
		}
	}
	else if (cmd == "TOPIC")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeTopic(clientFd, args);
		}
	}
	else if (cmd == "KICK")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeKick(clientFd, args);
		}
	}
	else if (cmd == "INVITE")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeInvite(clientFd, args);
		}
	}
	else if (cmd == "MODE")
	{
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered");
		} else {
			_executeMode(clientFd, args);
		}
	}
	else {
		// Si intenta usar cualquier otro comando sin estar registrado, lo ignoramos o mandamos error
		if (!_clients.at(clientFd).isRegistered()) {
			_sendNumericReply(clientFd, "451", "You have not registered"); // ERR_NOTREGISTERED
		} else {
			// Aquí irán el resto de comandos (JOIN, PRIVMSG...) más adelante
			_sendNumericReply(clientFd, "421", cmd + " :Unknown command"); // ERR_UNKNOWNCOMMAND
		}
	}
}

// ##EXECTUE

void Server::_executeNick(int clientFd, const std::vector<std::string>& args)
{
	if (args.empty() || args[0].empty()) {
		_sendNumericReply(clientFd, "431", "No nickname given"); // ERR_NONICKNAMEGIVEN
		return;
	}

	std::string newNick = args[0];

	// Verificar si el nickname ya está en uso por otro cliente
	std::map<int, Client>::iterator it;
	for (it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second.getNickname() == newNick) {
			_sendNumericReply(clientFd, "433", newNick + " :Nickname is already in use"); // ERR_NICKNAMEINUSE
			return;
		}
	}

	// Asignar el nuevo nickname
	_clients.at(clientFd).setNickname(newNick);
	std::cout << "fd " << clientFd << " cambió su nickname a: " << newNick << std::endl;

	// Si ya tiene USER y PASS correctos, intentamos registrarlo definitivamente
	// (Crearemos una función auxiliar para verificar esto en un momento)
}

void Server::_executeUser(int clientFd, const std::vector<std::string>& args) 
{
	Client& client = _clients.at(clientFd);

	if (client.isRegistered()) {
		_sendNumericReply(clientFd, "462", "You may not reregister"); // ERR_ALREADYREGISTRED
		return;
	}

	if (args.size() < 4) {
		_sendNumericReply(clientFd, "461", "USER :Not enough parameters"); // ERR_NEEDMOREPARAMS
		return;
	}

	// El primer argumento es el username
	client.setUsername(args[0]);
	std::cout << "fd " << clientFd << " configuró su username a: " << args[0] << std::endl;

	// Aquí ya podemos verificar si el cliente cumple las condiciones para ser bienvenido
	if (!client.getNickname().empty() && !client.getUsername().empty()) {
		client.setRegistered(true);
		// Enviamos el mensaje de bienvenida oficial del protocolo IRC (RPL_WELCOME)
		_sendNumericReply(clientFd, "001", "Welcome to the Internet Relay Network " + client.getNickname());
		std::cout << "Cliente fd " << clientFd << " se ha REGISTRADO con éxito." << std::endl;
	}
}

void Server::_executeJoin(int clientFd, const std::vector<std::string>& args)
{
	if (args.empty()) {
		_sendNumericReply(clientFd, "461", "JOIN :Not enough parameters");
		return;
	}

	std::string channelName = args[0];

	// El protocolo IRC exige que los canales empiecen por '#'
	if (channelName[0] != '#') {
		channelName = "#" + channelName;
	}

	Client& client = _clients.at(clientFd);

	// 1. Buscar si el canal ya existe en el mapa del servidor
	std::map<std::string, Channel>::iterator it = _channels.find(channelName);

	// if (it == _channels.end()) {
	// 	// SI NO EXISTE: Lo creamos e insertamos en el servidor
	// 	Channel newChannel(channelName);
	// 	_channels.insert(std::make_pair(channelName, newChannel));
		
	// 	// Ahora volvemos a apuntar el iterador al canal recién creado
	// 	it = _channels.find(channelName);
		
	// 	// Al ser el creador, lo hacemos Operador del canal
	// 	it->second.addOperator(&client);
	// 	std::cout << "Canal " << channelName << " creado por fd " << clientFd << " (Operador)." << std::endl;
	// }

	if (it == _channels.end()) {
        // Canal nuevo: crearlo y hacer al cliente operador
        Channel newChannel(channelName);
        _channels.insert(std::make_pair(channelName, newChannel));
        it = _channels.find(channelName);
        it->second.addMember(&client);          // <-- añadir ANTES de hacerlo operador
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
                _sendNumericReply(clientFd, "475", channelName + " :Cannot join channel (+k)");
                return;
            }
        }

        // Comprobar +l (límite de usuarios)
        if (it->second.getUserLimit() > 0 && it->second.getMemberCount() >= it->second.getUserLimit()) {
            _sendNumericReply(clientFd, "471", channelName + " :Cannot join channel (+l)");
            return;
        }

        it->second.addMember(&client);  // <-- añadir solo si pasa todas las comprobaciones
    }

	// 2. Añadir al cliente como miembro del canal
	it->second.addMember(&client);
	// Si el canal ya existe, comprobamos si está en modo Invite-Only y si el usuario está invitado
	if (it != _channels.end()) {
		if (it->second.isInviteOnly() && !it->second.isInvited(client.getNickname())) {
			_sendNumericReply(clientFd, "473", channelName + " :Cannot join channel (+i)"); // ERR_INVITEONLYCHAN
			return;
		}
	}

	if (it != _channels.end()) {
		// Chequeo del modo Invite-Only (+i)
		if (it->second.isInviteOnly() && !it->second.isInvited(client.getNickname())) {
			_sendNumericReply(clientFd, "473", channelName + " :Cannot join channel (+i)");
			return;
		}

		// --- NUEVO: Chequeo del modo Contraseña del Canal (+k) ---
		if (!it->second.getKey().empty()) {
			// El segundo argumento del JOIN sería la clave enviada (Sintaxis: JOIN <canal> <clave>)
			std::string providedKey = (args.size() > 1) ? args[1] : "";
			if (providedKey != it->second.getKey()) {
				_sendNumericReply(clientFd, "475", channelName + " :Cannot join channel (+k) - bad key");
				return;
			}
		}
	}

	// 3. Notificar al canal entero que alguien ha entrado (Formato oficial de IRC)
	// Formato: :<nick>!<user>@localhost JOIN :<canal>\r\n
	std::string joinMsg = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN :" + channelName + "\r\n";
	it->second.broadcast(joinMsg); // Esto se lo envía a todos los miembros, incluido el nuevo

	// 4. Opcional pero muy recomendado: Enviar el TOPIC actual de la sala al entrar (RPL_TOPIC = 332)
	if (it->second.getTopic().empty()) {
		_sendNumericReply(clientFd, "331", channelName + " :No topic is set");
	} else {
		_sendNumericReply(clientFd, "332", channelName + " :" + it->second.getTopic());
	}
}

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