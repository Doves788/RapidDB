#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>

class Command_Handler {
public:
    Command_Handler();
    // Process a command from a client and return a RESP-formatted response.
    std::string processCommand(const std::string& commandLine);
};

#endif