#include "KVDB.h"
#include "log.h"


std::vector<std::string> db::KVDB::parseRESP(const std::string& resp) {
	std::vector<std::string> tokens;
	size_t n = resp.size();
	for (size_t i = 0; i < n; ) {
		size_t j = i;
		for( ; j < n - 1; ++j) {
			if ('\r' == resp[j] && '\n' == resp[j + 1]) {
				break;
			}
		}
		tokens.push_back(resp.substr(i, j - i));
		i = j + 2;
	}
	return tokens;
}

db::RESPType db::KVDB::getResponseType(const std::string& token) {
    if (token.empty()) {
        return RESPType::Error;
    }

    char firstChar = token[0];

    switch (firstChar) {
        case '+':
            return db::RESPType::SimpleString;
        case '-':
            return db::RESPType::Error;
        case ':':
            return db::RESPType::Integer;
        case '$':
            return db::RESPType::BulkString;
        case '*':
            return db::RESPType::Array;
        default:
            return db::RESPType::Error;
    }
}

std::vector<std::string> db::KVDB::getRESPCommand(std::vector<std::string> & tokens) {
	std::vector<std::string> cmd;
	if (tokens.empty()) {
		return cmd;

	}
	if (db::RESPType::Array != getResponseType(tokens[0])) {
		return cmd;
	}
	for (size_t i = 1, n = tokens.size(); i < n; ++i) {
		if (db::RESPType::BulkString == getResponseType(tokens[i])) {
			if (i + 1 < n && std::stoi(tokens[i].substr(1)) == tokens[i + 1].size()) {
				cmd.push_back(tokens[++i]);
			} else if (i + 1 >= n && std::stoi(tokens[i].substr(1)) == 0) {
				cmd.emplace_back();
			} else {
				return cmd;
			}
		}
	}
	return cmd;
}

std::string db::KVDB::executeRESPCommand(std::vector<std::string> & cmd) {
	std::string result = "-ERROR\r\n";
	if (cmd.empty()) {
		ERROR("invalid command");
		return result;
	}
	if ("SET" == cmd[0] && 3 <= cmd.size()) {
		std::string key = cmd[1];
		std::string value;
		for (size_t i = 2, n = cmd.size(); i < n; ++i) {
			if (i + 1 < n) {
				value += cmd[i] + " ";
			} else {
				value += cmd[i];
			}
		}
		storage[key] = value;
		std::cout << key << " = " << value << std::endl;
		result = "+OK\r\n";
	} else if ("GET" == cmd[0] && 2 == cmd.size()) {
		std::cout << cmd[0] << " " << cmd[1] << std::endl;
		if (storage.count(cmd[1]) > 0) {
			std::cout << cmd[1] << " : " << storage[cmd[1]] << std::endl;
			result = "$" + std::to_string(storage[cmd[1]].size()) + "\r\n" + storage[cmd[1]] + "\r\n";
		} else {
			result = "-ERROR\r\n";
		}
	} /* else if ("DEL" == cmd && cmd.size() > 1) {
		int num = 0;
		for (size_t i = 1, n = cmd.size(); i < n; ++i) {
			if (storage.erase(cmd[i])) {
				++num;
			}
		}
		result = ":" + std::to_string(num) + "\r\n";
	}*/
		return result;
}

std::string db::KVDB::commit(const std::string& resp) {
	std::vector<std::string> tokens = parseRESP(resp);
	std::vector<std::string> cmd = getRESPCommand(tokens);
	return executeRESPCommand(cmd);
}