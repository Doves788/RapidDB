#include "../include/Server.h"
#include "../include/Command_Handler.h"
#include "../include/Database.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <thread>
#include <cstring>
#include <signal.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// Global pointer for signal handling
static Server* globalServer = nullptr;

void signalHandler(int signum) {
    if (globalServer) {
        std::cout << "\nCaught signal " << signum << ", shutting down...\n";
        globalServer->shutdown();
    }
    exit(signum);
}

void Server::setupSignalHandler() {
    signal(SIGINT, signalHandler);
#ifdef SIGTERM
    signal(SIGTERM, signalHandler);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif
}

Server::Server(int port) : port(port), server_socket(-1), running(true) {
    globalServer = this;
    setupSignalHandler();
}

void Server::shutdown() {
    running = false;
    if (server_socket != -1) {
        // Before shutdown, persist the database
        if (Database::getInstance().dump("dump.rapiddb"))
            std::cout << "Database Dumped to dump.rapiddb\n";
        else 
            std::cerr << "Error dumping database\n";
        close(server_socket);
        server_socket = -1;
    }
    std::cout << "Server Shutdown Complete!\n";
}

void Server::run() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error Creating Server Socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error Binding Server Socket\n";
        return;
    }

    if (listen(server_socket, 128) < 0) {
        std::cerr << "Error Listening On Server Socket\n";
        return;
    } 

    std::cout << "RapidDB Server Listening On Port " << port << "\n";

    Command_Handler cmdHandler;

    while (running) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket < 0) {
            if (running) 
                std::cerr << "Error Accepting Client Connection\n";
            break;
        }

        std::thread([client_socket, &cmdHandler](){
            std::string connectionBuffer;
            char buffer[8192];
            while (true) {
                int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes <= 0) break;
                connectionBuffer.append(buffer, bytes);

                while (!connectionBuffer.empty()) {
                    size_t bytesConsumed = 0;
                    std::string response = cmdHandler.processBuffer(connectionBuffer, bytesConsumed);
                    if (bytesConsumed == 0) {
                        // Incomplete command, wait for more data from socket
                        break;
                    }
                    if (!response.empty()) {
                        ssize_t sent = send(client_socket, response.c_str(), response.size(), MSG_NOSIGNAL);
                        if (sent < 0) {
                            connectionBuffer.clear();
                            break;
                        }
                    }
                    if (response.rfind("-Error: Protocol error", 0) == 0) {
                        connectionBuffer.clear();
                        break;
                    }
                    connectionBuffer.erase(0, bytesConsumed);
                }
            }
            close(client_socket);
        }).detach();
    }

    // Before shutdown, persist the database
    if (Database::getInstance().dump("dump.rapiddb"))
        std::cout << "Database Dumped to dump.rapiddb\n";
    else 
        std::cerr << "Error dumping database\n";
}
