#include "../../include/Server.hpp"

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

	client.setValidPassword(true);
	// Si la contraseña es correcta, guardamos un estado interno o simplemente dejamos que prosiga.
	// Ojo: No lo marcamos como 'isRegistered = true' todavía, porque le falta NICK y USER.
	std::cout << "fd " << clientFd << " superó el chequeo de contraseña con éxito." << std::endl;
}