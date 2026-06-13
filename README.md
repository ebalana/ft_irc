# ft_irc

> This project has been created as part of the 42 curriculum by `ebalana-` and `dcampas-`

## Description

`ft_irc` is a custom **Internet Relay Chat (IRC) server** written from scratch in **C++98**, developed as part of the 42 Common Core.

The goal of the project is to implement the core of the IRC protocol: a TCP server able to handle multiple clients at the same time using a single `poll()` loop, with non-blocking I/O and no forking. The server allows real IRC clients to connect, authenticate, choose a nickname and username, join channels, exchange public and private messages, and manage channels through operator commands and modes.

The project does **not** implement an IRC client and does **not** implement server-to-server communication, as required by the subject.

## Features

- Non-blocking TCP server handled with a single `poll()` loop.
- Support for multiple simultaneous clients.
- Authentication via server password (`PASS`).
- User registration (`NICK`, `USER`).
- Channel management (creation, joining, members, operators).
- Public (channel) and private messaging (`PRIVMSG`).
- Topic management with operator restrictions (`TOPIC`).
- Kicking users from a channel (`KICK`).
- Inviting users to a channel (`INVITE`).
- Channel modes (`MODE`): `i`, `t`, `k`, `l`, `o`.
- Partial/fragmented message handling (buffered input, reassembled on `\n`).
- The server never crashes or exits unexpectedly, even on malformed input.

## Architecture overview

- **`Server`**: Creates and configures the listening socket, runs the main `poll()` event loop, accepts new connections, reads and buffers client input, splits it into complete lines, and dispatches commands to the relevant handler.
- **`Client`**: Represents a connected user (file descriptor, nickname, username, input buffer, registration state).
- **`Channel`**: Represents an IRC channel (name, topic, key, user limit, members, operators, invite list, and broadcasting to members).

### Supported commands

| Command   | Description |
|-----------|-------------|
| `PASS`    | Authenticates the client with the server password. |
| `NICK`    | Sets or changes the client's nickname. |
| `USER`    | Sets the username and completes client registration. |
| `JOIN`    | Joins (or creates) a channel. Supports channel key (`+k`) and invite-only (`+i`). |
| `PRIVMSG` | Sends a message to a user or to a channel. |
| `TOPIC`   | Views or changes a channel's topic. |
| `KICK`    | Removes a user from a channel (operators only). |
| `INVITE`  | Invites a user to a channel. |
| `MODE`    | Changes channel modes: `i`, `t`, `k`, `l`, `o`. |

### Channel modes (`MODE`)

| Mode | Meaning |
|------|---------|
| `i`  | Invite-only channel. |
| `t`  | Restricts the `TOPIC` command to channel operators. |
| `k`  | Sets/removes the channel key (password). |
| `l`  | Sets/removes the user limit of the channel. |
| `o`  | Gives/takes channel operator privilege. |


## Instructions

### Compilation

```bash
make
```

This compiles the project with `c++` using the flags `-Wall -Wextra -Werror -std=c++98` and produces the `ircserv` executable.

Other available rules:

```bash
make clean   # removes object files
make fclean  # removes object files and the binary
make re      # fclean + make (rebuilds everything)
```

### Usage

```bash
./ircserv <port> <password>
```

- `port`: the TCP port the server will listen on (1024–65535).
- `password`: the connection password that any IRC client must provide via `PASS` before registering.

Example:

```bash
./ircserv 6667 mypassword
```

### Connecting

The server can be tested with `nc` (**Netcat**):

```bash
nc -C 127.0.0.1 6667
```

It can also be tested with a real IRC client such as **Irssi**, **HexChat**, or **WeeChat**:

```bash
/connect 127.0.0.1 6667
/pass mypassword
/nick myNick
/user myUser 0 * :My Real Name
```

To simulate fragmented input (as described in the subject), data can be sent in several chunks using `Ctrl+D` with `nc -C`.

### Bonus: File Transfer (DCC) Netcat to Netcat

**Step 1: Host the file**
Sender must open a **new terminal window** and inject the file into it.
```bash
nc  -l 5000 -q 0 < myfile.txt
```
*Use `-q 0` on Linux to auto-close the connection when the transfer finishes.*

**Step 2: Send the file**
In the sender's Netcat session, send the DCC offer to the receiver
```bash
PRIVMSG david :DCC SEND myfile.txt 2130706433 55001 myfile_bytes
```

**Step 3: Read the offer and download**
The receiver (`david`) will see the exact raw message appear in their Netcat session:
```bash
ebalana-!ebalana-@localhost PRIVMSG david :DCC SEND myfile.txt 2130706433 55001 myfile_bytes
```
The receiver opens a **new terminal window**, connects to the provided IP and port, and redirects the output to save the file:
```bash
nc 127.0.0.1 5000 > downloaded_myfile.txt
```

### Bonus: File Transfer (DCC) Irssi to Netcat

**Step 1: Send the file from irssi**
The sender must use the built-in DCC command in their `irssi` client:
```bash 
/dcc send david myfile.txt
```

**Step 2: Intercept the handshake in Netcat**
The receiver (david) will see the raw IRC protocol offer printed in their Netcat session:
```bash 
:ebalana-!ebalana-@localhost PRIVMSG david : DCC SEND myfile.txt 2130706433 55000 myfile_bytes
```
*The long number `2130706433` is the IP `127.0.0.1` and `55000` is the dynamic port generated by irssi.*

**Step 3: Download the file manually**
The receiver opens a **new terminal window**, connects to the provided IP and port, and redirects the output to save the file:
```bash  
nc 127.0.0.1 55000 > downloaded_myfile.txt
```

## Project structure

```
ft_irc/
├── include/
│   ├── Server.hpp
│   ├── Client.hpp
│   └── Channel.hpp
├── src/
│   ├── Channel.cpp
│   ├── Client.cpp
│   ├── main.cpp
│   ├── Server.cpp
│   └── commands/
│       ├── Pass.cpp
│       ├── Nick.cpp
│       ├── User.cpp
│       ├── Join.cpp
│       ├── Privmsg.cpp
│       ├── Topic.cpp
│       ├── Kick.cpp
│       ├── Invite.cpp
│       └── Mode.cpp
├── Makefile
└── README.md
```

## Resources

### IRC protocol references

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [modern.ircdocs.info — Modern IRC Client Protocol](https://modern.ircdocs.info/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man poll`, `man fcntl`, `man socket`


### AI usage

AI (Claude/Gemini) was used to reduce repetitive tasks and assist with the following specific areas:

* Network Debugging: Assisting in identifying the root cause of an infinite loop caused by client CTRL+D disconnections. The AI helped clarify the relationship between recv() returning 0, the POLLHUP flag in poll(), and safe file descriptor cleanup.

* Architecture Refactoring: Providing a clean directory structure blueprint (core/, network/, commands/) and updating the Makefile to transition away from a single bloated Server.cpp file into isolated command modules.

* Testing Automation: Generating a bash testing script (test_modes.sh) to automate repetitive Netcat connections and validate the strict behavior of all required channel modes (+i, +k, +l, +o, +t).

* All AI-generated insights were reviewed, tested manually with nc and a reference client, and thoroughly understood to take full responsibility during the evaluation phase.
