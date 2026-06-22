#include "../include/Server.h"
#include "../include/Database.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    int port = 6379; // default
    if (argc >= 2) port = std::stoi(argv[1]); 

    if (Database::getInstance().load("dump.rapiddb"))
        std::cout << "Database Loaded From dump.rapiddb\n";
    else 
        std::cout << "No dump found or load failed; starting with an empty database.\n";

    Server server(port);

    // Background persistence: dump the database every 300 seconds. (5 * 60 seconds save database)
    std::thread persistanceThread([](){
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(300));
            if (!Database::getInstance().dump("dump.rapiddb"))
                std::cerr << "Error Dumping Database\n";
            else 
                std::cout << "Database Dumped to dump.rapiddb\n";
        }
    });
    persistanceThread.detach();
    
    server.run();
    return 0;
}