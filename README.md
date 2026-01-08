# Unix Systems Programming

This repository contains a collection of system-level projects developed for the Computer Science program at Vilnius University. The implementations focus on POSIX-compliant shell scripting, process management, and network programming using the BSD Socket API.

## Project Overview

### Lab 1: POSIX Shell URL Validator
A non-interactive script designed to validate URLs based on specific patterns.
- Implementation: Pure /bin/sh (POSIX compliant).
- Constraints: Developed without the use of variables, loops, or conditional statements, relying entirely on shell case-matching and pipes.
- Input: Accepts command-line arguments as per Unix utility standards.

### Lab 2: Pipeline Shell Implementation
A custom interactive shell environment designed to handle process creation and inter-process communication.
- Core Functions: fork(2), execvp(3), waitpid(2), pipe(2), dup2(2).
- Features: 
  - Full support for command pipelines (command1 | command2).
  - Implementation of a custom command parser with whitespace trimming.
  - Interactive prompt with support for EOF (Ctrl+D) and "exit" command.
- Requirement Compliance: Zero reliance on the system(3) function.

### Lab 3: Multi-client TCP Chat
A client-server messaging architecture utilizing asynchronous I/O multiplexing.
- Sockets API: socket(2), bind(2), listen(2), accept(2), connect(2), send(2), recv(2), close(2), shutdown(2).
- Network Features: 
  - I/O multiplexing using select(2) to handle up to 20 concurrent clients without blocking.
  - Network name resolution and byte order conversion (hton, ntoh).
  - Implementation of timestamps, private messaging (/msg), and user listings (/list).

## Requirements Compliance
- All C code is implemented using standard Unix APIs without external process management libraries.
- Memory management is handled via standard allocation (malloc/free).
- The projects are compatible with standard Linux environments.

## Installation and Execution

### Building the Shell (Lab 2)
cd lab2-unix-process-management
make
./pipeline_shell

### Building the Chat (Lab 3)
cd lab3-tcp-chat
gcc chat_server.c -o chat_server
gcc chat_client.c -o chat_client

## License
This project is licensed under the MIT License.
