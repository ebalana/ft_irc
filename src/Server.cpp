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

	while (true) {
		int pollCount = poll(&_pollFds[0], _pollFds.size(), -1);
		
		if (pollCount == -1) {
			throw std::runtime_error("Error en la llamada a poll().");
		}

		// Recorremos de atrás hacia adelante para poder borrar elementos de forma segura
		for (int i = (int)_pollFds.size() - 1; i >= 0; i--) {
			
			if (_pollFds[i].revents == 0)
				continue;

			// CASO 1: Nueva Conexión en el socket principal del servidor
			if (_pollFds[i].fd == _serverSocketFd) {
				if (_pollFds[i].revents & POLLIN) {
					_handleNewConnection();
				}
				continue; // Importante saltar al siguiente evento
			} 

			// CASO 2: Actividad en un cliente existente
			// A) Primero verificamos si hay desconexión o error (CTRL+D que cierra socket)
			if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				int clientFd = _pollFds[i].fd;
				std::cout << "Desconexión detectada por poll() en fd: " << clientFd << std::endl;
				
				_leaveAllChannels(clientFd);
				close(clientFd);
				_clients.erase(clientFd);
				_pollFds.erase(_pollFds.begin() + i);
				continue; // Socket borrado, pasamos al siguiente
			}
			
			// B) Si no hay error, leemos los datos
			if (_pollFds[i].revents & POLLIN) {
				_handleClientData(i);
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
	int clientFd = _pollFds[index].fd;
	char buffer[1024];
	std::memset(buffer, 0, sizeof(buffer));

	int bytesReceived = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

	// 1. Verificar si el cliente cerró la conexión físicamente
	if (bytesReceived <= 0) 
	{
		if (bytesReceived < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return; // socket no-bloqueante sin datos, no es error real
		if (bytesReceived == 0)
			std::cout << "Cliente en fd " << clientFd << " se ha desconectado." << std::endl;
		else
			std::cerr << "Error en recv() con el fd " << clientFd << std::endl;
		_leaveAllChannels(clientFd);
		close(clientFd);
		_pollFds.erase(_pollFds.begin() + index);
		_clients.erase(clientFd);
		return;
	}

	// 2. Acumulamos EXACTAMENTE los bytes recibidos en el buffer del cliente
	_clients.at(clientFd).appendToBuffer(std::string(buffer, bytesReceived));

	std::string& clientBuffer = _clients.at(clientFd).getBuffer();
	size_t pos;

	// 3. Procesamos SOLO si encontramos un salto de línea (\n)
	// Si el usuario hace CTRL+D sin Enter, esto se salta y espera de forma segura
	while ((pos = clientBuffer.find('\n')) != std::string::npos) {
		
		std::string command = clientBuffer.substr(0, pos);
		
		// Limpiar el \r si el cliente es nc -C o un cliente IRC real
		if (!command.empty() && command[command.size() - 1] == '\r') {
			command.erase(command.size() - 1);
		}

		// Borrar el comando procesado del buffer (INCLUYENDO el \n)
		clientBuffer.erase(0, pos + 1);

		// Ejecutar comando
		if (!command.empty()) {
			std::cout << "Ejecutando desde fd " << clientFd << ": [" << command << "]" << std::endl;
			_processCommand(clientFd, command);
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