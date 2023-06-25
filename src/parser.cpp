#include "parser.h"

std::vector<std::string> raft::split_cmd(const std::string & msg) {
	std::vector<std::string> result;
    size_t start = 0;
    for (size_t i = 0; i < msg.length() - 1; ++i) {
        if (msg[i] == '\r' && msg[i+1] == '\n') {
            std::string sub = msg.substr(start, i - start);
            if (sub[0] != '*' && sub[0] != '$') {
                result.push_back(sub);
            }
            start = i + 2;
        }
    }
    return result;
}