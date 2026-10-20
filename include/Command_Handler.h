#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <cstddef>

class Command_Handler {
public:
    Command_Handler();
    // Process a command from a client and return a RESP-formatted response.
    std::string processCommand(const std::string& commandLine);

    // Process buffered stream data; returns the RESP response and sets bytesConsumed.
    // If input is incomplete, returns an empty string and sets bytesConsumed = 0.
    std::string processBuffer(const std::string& buffer, size_t& bytesConsumed);
};

#endif
