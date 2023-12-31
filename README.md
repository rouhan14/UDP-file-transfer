# UDP File Transfer Application

This is a simple UDP-based file transfer application that allows a client to send a file to a server using a sliding window mechanism.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Usage](#usage)
- [Architecture](#architecture)

## Features

- File transfer over UDP protocol.
- Sliding window mechanism for efficient data transmission.
- Basic error handling for connection issues.
- Client and server applications.

## Requirements

- C compiler (e.g., GCC)
- POSIX-compliant operating system (Linux, macOS, etc.)

## Usage

1. **Compile the Code:**

    ```bash
    gcc -o udp_client udp_client.c -pthread
    gcc -o udp_server udp_server.c -pthread
    ```

2. **Run the Server:**

    ```bash
    ./udp_server
    ```

3. **Run the Client:**

    ```bash
    ./udp_client <hostname> <filename>
    ```

    Replace `<hostname>` with the server's IP address or hostname and `<filename>` with the name of the file you want to transfer.

## Architecture

The application consists of two main components: the UDP Client and the UDP Server.

- **UDP Client:**
  - Responsible for establishing a connection with the server.
  - Sends the file size to the server.
  - Divides the file into packets and sends them using a sliding window.
  - Handles acknowledgment from the server and resends packets if needed.

- **UDP Server:**
  - Listens for incoming connections from clients.
  - Receives the file size from the client.
  - Creates a buffer for receiving packets and saves them to a local file.
  - Sends acknowledgments to the client.
