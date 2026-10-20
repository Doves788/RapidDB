#include "../include/Command_Handler.h"
#include "../include/Database.h"

#include <vector>
#include <sstream>
#include <algorithm>
#include <exception>
#include <cctype>

// Helper to safely parse an integer
static bool safeParseInt(const std::string& str, int& out) {
    if (str.empty()) return false;
    try {
        size_t idx = 0;
        long long val = std::stoll(str, &idx);
        if (idx != str.size()) return false;
        if (val < -2147483648LL || val > 2147483647LL) return false;
        out = static_cast<int>(val);
        return true;
    } catch (...) {
        return false;
    }
}

// Attempts to parse one full command from buffer.
// Returns:
//   true  -> either parsed successfully (tokens populated) or malformed (isMalformed = true)
//   false -> need more data (incomplete frame)
static bool parseRespFromBuffer(const std::string &input, std::vector<std::string> &tokens, size_t &bytesConsumed, bool &isMalformed) {
    tokens.clear();
    bytesConsumed = 0;
    isMalformed = false;
    if (input.empty()) return false;

    // Inline command fallback (does not start with '*')
    if (input[0] != '*') {
        size_t newline = input.find('\n');
        if (newline == std::string::npos) {
            return false; // wait for full line
        }
        bytesConsumed = newline + 1;
        std::string line = input.substr(0, newline);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream iss(line);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return true;
    }

    // Multi-bulk array: *<numElements>\r\n...
    size_t pos = 1;
    size_t crlf = input.find("\r\n", pos);
    if (crlf == std::string::npos) {
        return false; // need more data
    }

    int numElements = 0;
    if (!safeParseInt(input.substr(pos, crlf - pos), numElements) || numElements < 0 || numElements > 100000) {
        isMalformed = true;
        bytesConsumed = crlf + 2;
        return true;
    }

    pos = crlf + 2;
    for (int i = 0; i < numElements; i++) {
        if (pos >= input.size()) return false;
        if (input[pos] != '$') {
            isMalformed = true;
            bytesConsumed = pos + 1;
            return true;
        }
        pos++;

        crlf = input.find("\r\n", pos);
        if (crlf == std::string::npos) return false;

        int len = 0;
        if (!safeParseInt(input.substr(pos, crlf - pos), len) || len < 0 || len > 100000000) {
            isMalformed = true;
            bytesConsumed = crlf + 2;
            return true;
        }

        pos = crlf + 2;
        if (pos + static_cast<size_t>(len) + 2 > input.size()) {
            return false; // bulk string payload or CRLF hasn't arrived completely yet
        }

        tokens.push_back(input.substr(pos, len));
        pos += len;

        if (input[pos] != '\r' || input[pos + 1] != '\n') {
            isMalformed = true;
            bytesConsumed = pos + 2;
            return true;
        }
        pos += 2;
    }

    bytesConsumed = pos;
    return true;
}

//----------------------
// Common Commands
//----------------------
static std::string handlePing(const std::vector<std::string>& /*tokens*/, Database& /*db*/) {
    return "+PONG\r\n";
}

static std::string handleEcho(const std::vector<std::string>& tokens, Database& /*db*/) {
    if (tokens.size() < 2)
        return "-Error: ECHO requires a message\r\n";
    return "+" + tokens[1] + "\r\n";
}

static std::string handleFlushAll(const std::vector<std::string>& /*tokens*/, Database& db) {
    db.flushAll();
    return "+OK\r\n";
}

//----------------------
// Key/Value Operations
//----------------------
static std::string handleSet(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3)
        return "-Error: SET requires key and value\r\n";
    db.set(tokens[1], tokens[2]);
    return "+OK\r\n";
}

static std::string handleGet(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2)
        return "-Error: GET requires key\r\n";
    std::string value;
    if (db.get(tokens[1], value))
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    return "$-1\r\n";
}

static std::string handleKeys(const std::vector<std::string>& /*tokens*/, Database& db) {
    auto allKeys = db.keys();
    std::ostringstream oss;
    oss << "*" << allKeys.size() << "\r\n";
    for (const auto& key : allKeys)
        oss << "$" << key.size() << "\r\n" << key << "\r\n";
    return oss.str();
}

static std::string handleType(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2)
        return "-Error: TYPE requires key\r\n";
    return "+" + db.type(tokens[1]) + "\r\n";
}

static std::string handleDel(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2)
        return "-Error: DEL requires key\r\n";
    bool res = db.del(tokens[1]);
    return ":" + std::to_string(res ? 1 : 0) + "\r\n";
}

static std::string handleExpire(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3)
        return "-Error: EXPIRE requires key and time in seconds\r\n";
    int seconds = 0;
    if (!safeParseInt(tokens[2], seconds)) {
        return "-Error: Invalid expiration time\r\n";
    }
    if (db.expire(tokens[1], seconds))
        return "+OK\r\n";
    else
        return "-Error: Key not found\r\n";
}

static std::string handleRename(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3)
        return "-Error: RENAME requires old key and new key\r\n";
    if (db.rename(tokens[1], tokens[2]))
        return "+OK\r\n";
    return "-Error: Key not found or rename failed\r\n";
}

//----------------------
// List Operations
//----------------------
static std::string handleLget(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2)
        return "-Error: LGET requires a key\r\n";

    auto elems = db.lget(tokens[1]);
    std::ostringstream oss;
    oss << "*" << elems.size() << "\r\n";
    for (const auto& e : elems) {
        oss << "$" << e.size() << "\r\n"
            << e << "\r\n";
    }
    return oss.str();
}

static std::string handleLlen(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: LLEN requires key\r\n";
    long long len = db.llen(tokens[1]);
    return ":" + std::to_string(len) + "\r\n";
}

static std::string handleLpush(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: LPUSH requires key and value\r\n";
    for (size_t i = 2; i < tokens.size(); ++i) {
        db.lpush(tokens[1], tokens[i]);
    }
    long long len = db.llen(tokens[1]);
    return ":" + std::to_string(len) + "\r\n";
}

static std::string handleRpush(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: RPUSH requires key and value\r\n";
    for (size_t i = 2; i < tokens.size(); ++i) {
        db.rpush(tokens[1], tokens[i]);
    }    
    long long len = db.llen(tokens[1]);
    return ":" + std::to_string(len) + "\r\n";
}

static std::string handleLpop(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: LPOP requires key\r\n";
    std::string val;
    if (db.lpop(tokens[1], val))
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    return "$-1\r\n";
}

static std::string handleRpop(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: RPOP requires key\r\n";
    std::string val;
    if (db.rpop(tokens[1], val))
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    return "$-1\r\n";
}

static std::string handleLrem(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 4) 
        return "-Error: LREM requires key, count and value\r\n";
    int count = 0;
    if (!safeParseInt(tokens[2], count)) {
        return "-Error: Invalid count\r\n";
    }
    int removed = db.lrem(tokens[1], count, tokens[3]);
    return ":" + std::to_string(removed) + "\r\n";
}

static std::string handleLindex(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: LINDEX requires key and index\r\n";
    int index = 0;
    if (!safeParseInt(tokens[2], index)) {
        return "-Error: Invalid index\r\n";
    }
    std::string value;
    if (db.lindex(tokens[1], index, value)) 
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    else 
        return "$-1\r\n";
}

static std::string handleLset(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 4) 
        return "-Error: LSET requires key, index and value\r\n";
    int index = 0;
    if (!safeParseInt(tokens[2], index)) {
        return "-Error: Invalid index\r\n";
    }
    if (db.lset(tokens[1], index, tokens[3]))
        return "+OK\r\n";
    else 
        return "-Error: Index out of range\r\n";
}

//----------------------
// Hash Operations
//----------------------
static std::string handleHset(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 4) 
        return "-Error: HSET requires key, field and value\r\n";
    db.hset(tokens[1], tokens[2], tokens[3]);
    return ":1\r\n";
}

static std::string handleHget(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: HGET requires key and field\r\n";
    std::string value;
    if (db.hget(tokens[1], tokens[2], value))
        return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
    return "$-1\r\n";
}

static std::string handleHexists(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: HEXISTS requires key and field\r\n";
    bool exists = db.hexists(tokens[1], tokens[2]);
    return ":" + std::to_string(exists ? 1 : 0) + "\r\n";
}

static std::string handleHdel(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 3) 
        return "-Error: HDEL requires key and field\r\n";
    bool res = db.hdel(tokens[1], tokens[2]);
    return ":" + std::to_string(res ? 1 : 0) + "\r\n";
}

static std::string handleHgetall(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: HGETALL requires key\r\n";
    auto hash = db.hgetall(tokens[1]);
    std::ostringstream oss;
    oss << "*" << hash.size() * 2 << "\r\n";
    for (const auto& pair: hash) {
        oss << "$" << pair.first.size() << "\r\n" << pair.first << "\r\n";
        oss << "$" << pair.second.size() << "\r\n" << pair.second << "\r\n";
    }
    return oss.str();
}

static std::string handleHkeys(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: HKEYS requires key\r\n";
    auto keys = db.hkeys(tokens[1]);
    std::ostringstream oss;
    oss << "*" << keys.size() << "\r\n";
    for (const auto& key: keys) {
        oss << "$" << key.size() << "\r\n" << key << "\r\n";
    }
    return oss.str();
}

static std::string handleHvals(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: HVALS requires key\r\n";
    auto values = db.hvals(tokens[1]);
    std::ostringstream oss;
    oss << "*" << values.size() << "\r\n";
    for (const auto& val: values) {
        oss << "$" << val.size() << "\r\n" << val << "\r\n";
    }
    return oss.str();
}

static std::string handleHlen(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 2) 
        return "-Error: HLEN requires key\r\n";
    long long len = db.hlen(tokens[1]);
    return ":" + std::to_string(len) + "\r\n";
}

static std::string handleHmset(const std::vector<std::string>& tokens, Database& db) {
    if (tokens.size() < 4 || (tokens.size() % 2) == 1) 
        return "-Error: HMSET requires key followed by field value pairs\r\n";
    std::vector<std::pair<std::string, std::string>> fieldValues;
    for (size_t i = 2; i < tokens.size(); i += 2) {
        fieldValues.emplace_back(tokens[i], tokens[i+1]);
    }
    db.hmset(tokens[1], fieldValues);
    return "+OK\r\n";
}

static std::string dispatchCommand(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return "-Error: Empty command\r\n";

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    Database& db = Database::getInstance();

    // Common Commands
    if (cmd == "PING")
        return handlePing(tokens, db);
    else if (cmd == "ECHO")
        return handleEcho(tokens, db);
    else if (cmd == "FLUSHALL")
        return handleFlushAll(tokens, db);
    // Key/Value Operations
    else if (cmd == "SET")
        return handleSet(tokens, db);
    else if (cmd == "GET")
        return handleGet(tokens, db);
    else if (cmd == "KEYS")
        return handleKeys(tokens, db);
    else if (cmd == "TYPE")
        return handleType(tokens, db);
    else if (cmd == "DEL" || cmd == "UNLINK")
        return handleDel(tokens, db);
    else if (cmd == "EXPIRE")
        return handleExpire(tokens, db);
    else if (cmd == "RENAME")
        return handleRename(tokens, db);
    // List Operations
    else if (cmd == "LGET") 
        return handleLget(tokens, db);
    else if (cmd == "LLEN") 
        return handleLlen(tokens, db);
    else if (cmd == "LPUSH")
        return handleLpush(tokens, db);
    else if (cmd == "RPUSH")
        return handleRpush(tokens, db);
    else if (cmd == "LPOP")
        return handleLpop(tokens, db);
    else if (cmd == "RPOP")
        return handleRpop(tokens, db);
    else if (cmd == "LREM")
        return handleLrem(tokens, db);
    else if (cmd == "LINDEX")
        return handleLindex(tokens, db);
    else if (cmd == "LSET")
        return handleLset(tokens, db);
    // Hash Operations
    else if (cmd == "HSET") 
        return handleHset(tokens, db);
    else if (cmd == "HGET") 
        return handleHget(tokens, db);
    else if (cmd == "HEXISTS") 
        return handleHexists(tokens, db);
    else if (cmd == "HDEL") 
        return handleHdel(tokens, db);
    else if (cmd == "HGETALL") 
        return handleHgetall(tokens, db);
    else if (cmd == "HKEYS") 
        return handleHkeys(tokens, db);
    else if (cmd == "HVALS") 
        return handleHvals(tokens, db);
    else if (cmd == "HLEN") 
        return handleHlen(tokens, db);
    else if (cmd == "HMSET") 
        return handleHmset(tokens, db);
    else 
        return "-Error: Unknown command\r\n";
}

Command_Handler::Command_Handler() {}

std::string Command_Handler::processBuffer(const std::string& buffer, size_t& bytesConsumed) {
    std::vector<std::string> tokens;
    bool isMalformed = false;
    if (!parseRespFromBuffer(buffer, tokens, bytesConsumed, isMalformed)) {
        bytesConsumed = 0;
        return "";
    }
    if (isMalformed) {
        return "-Error: Protocol error: invalid multi-bulk or bulk length\r\n";
    }
    return dispatchCommand(tokens);
}

std::string Command_Handler::processCommand(const std::string& commandLine) {
    size_t consumed = 0;
    std::string response = processBuffer(commandLine, consumed);
    if (consumed == 0 && !commandLine.empty()) {
        std::istringstream iss(commandLine);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) tokens.push_back(token);
        if (!tokens.empty()) return dispatchCommand(tokens);
    }
    return response.empty() ? "-Error: Incomplete command\r\n" : response;
}
