#ifndef _PARSER_H_
#define _PARSER_H_

#include <string>
#include <vector>

namespace raft{
	std::vector<std::string> split_cmd(const std::string & msg);
}


#endif