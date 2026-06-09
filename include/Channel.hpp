#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <vector>
#include "Client.hpp"
#include <sys/socket.h>
#include <algorithm>

class Channel 
{
	private:
		std::string           _name;
		std::string           _topic;
		std::string           _key;         // Para el modo +k (Contraseña del canal)
		size_t                _userLimit;   // Para el modo +l (Límite de usuarios)

		// Flags de modos del canal
		bool                  _inviteOnly;  // Modo +i
		bool                  _topicRestricted; // Modo +t (Solo operadores cambian topic)

		// Listas de clientes indexadas por su FD
		std::map<int, Client*> _members;
		std::map<int, Client*> _operators;
		std::vector<std::string> _invitedUsers; // Lista de nicks invitados (Modo +i)

	public:
		Channel(const std::string& name);
		~Channel();

		// Getters y Setters básicos
		std::string getName() const;
		std::string getTopic() const;
		void setTopic(const std::string& topic);

		// Gestión de miembros
		void addMember(Client* client);
		void removeMember(int fd);
		bool isMember(int fd) const;

		// Gestión de operadores
		void addOperator(Client* client);
		void removeOperator(int fd);
		bool isOperator(int fd) const;

		// Métodos para enviar mensajes grupales (Requisito del subject)
		void broadcast(const std::string& message, int excludeFd = -1);

		// Getters para los modos (los necesitaremos para el comando MODE)
		bool isInviteOnly() const;
		bool isTopicRestricted() const;
		std::string getKey() const;
		size_t getUserLimit() const;
		size_t getMemberCount() const;

		void addInvitation(const std::string& nick);
		bool isInvited(const std::string& nick) const;

		// Modificadores de modos del canal
		void setInviteOnly(bool value);
		void setTopicRestricted(bool value);
		void setKey(const std::string& key);
		void setUserLimit(size_t limit);
};

#endif