DEF_COLOR = \033[0;39m
YELLOW = \033[0;93m
CYAN = \033[0;96m
GREEN = \033[0;92m
RED = \033[0;91m

NAME = ircserv
CXX = c++
CXXFLAGS = -Werror -Wall -Wextra -std=c++98
RM = rm -f

HEADERS = include/Server.hpp include/Client.cpp include/Channel.hpp
SRCS = src/main.cpp  src/Server.cpp src/Client.cpp src/Channel.cpp src/Execute/Invite.cpp src/Execute/Join.cpp src/Execute/Kick.cpp src/Execute/Mode.cpp src/Execute/Nick.cpp src/Execute/Pass.cpp src/Execute/Privmsg.cpp src/Execute/Topic.cpp src/Execute/User.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

%.o: %.cpp $(HEADERS) Makefile
	@echo "$(YELLOW)Compiling: $<$(DEF_COLOR)"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	@echo "$(CYAN)Linking $(NAME)...$(DEF_COLOR)"
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)$(NAME) compiled!$(DEF_COLOR)"

clean:
	@$(RM) $(OBJS)
	@echo "$(RED)Cleaned object files$(DEF_COLOR)"

fclean: clean
	@$(RM) $(NAME)
	@echo "$(RED)Cleaned binary$(DEF_COLOR)"

re: fclean all

.PHONY: all clean fclean re