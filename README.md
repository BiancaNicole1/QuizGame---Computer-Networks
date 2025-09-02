Client-Server Application in C (IPC with FIFOs, Pipes, and Sockets)

This project implements a client-server application in C that uses FIFOs, pipes, and socketpairs for inter-process communication.
The server supports commands such as:

- login <username> <password> – authenticate user

- get-logged-users – list currently logged-in system users

- get-proc-info <PID> – show process information from /proc/[PID]/status

- logout – log out the current user

- quit – terminate session
