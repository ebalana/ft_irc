#include "../include/Server.hpp"

/**
 * Valida que el puerto sea un número y esté en el rango correcto (1024 - 65535).
 * Los puertos por debajo de 1024 requieren permisos de superusuario (root).
 */
bool isValidPort(const std::string& portStr) {
	if (portStr.empty()) return false;

	for (size_t i = 0; i < portStr.length(); i++) {
		if (!std::isdigit(portStr[i])) return false;
	}

	int port = std::atoi(portStr.c_str());
	return (port >= 1024 && port <= 65535);
}

int main(int argc, char** argv) {
	// Requisito del subject: ./ircserv <port> <password>
	if (argc != 3) {
		std::cerr << "Error: Uso correcto: ./ircserv <port> <password>" << std::endl;
		return 1;
	}

	if (!isValidPort(argv[1])) {
		std::cerr << "Error: El puerto debe ser un número entre 1024 y 65535." << std::endl;
		return 1;
	}

	int port = std::atoi(argv[1]);
	std::string password = argv[2];

	if (password.empty()) {
		std::cerr << "Error: La contraseña no puede estar vacía." << std::endl;
		return 1;
	}

	std::cout << "--- Iniciando ft_irc Servidor ---" << std::endl;

	try {
		// Aquí instanciamos e iniciamos nuestro servidor
		Server ircServer(port, password);
		ircServer.init(); // Configura los sockets
		ircServer.run();  // Bucle infinito de poll()
	} 
	catch (const std::exception& e) {
		// Requisito del subject: El programa no debe crashear bajo ninguna circunstancia
		std::cerr << "Error crítico del servidor: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}