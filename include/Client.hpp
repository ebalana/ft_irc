#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client 
{
	private:
		int			_fd;
		std::string	_nickname;
		std::string	_username;
		std::string	_buffer;       // El buffer donde acumularemos los retazos de texto
		bool		_isRegistered; // Para saber si ya completó el PASS, NICK y USER

	public:
		Client(int fd);
		~Client();

		// Getters y Setters
		int getFd() const;
		std::string getNickname() const;
		void setNickname(const std::string& nick);
		std::string getUsername() const;
		void setUsername(const std::string& user);
		
		// Gestión del Buffer
		std::string& getBuffer();
		void appendToBuffer(const std::string& str);
		void clearBuffer();

		bool isRegistered() const;
		void setRegistered(bool value);
};

#endif