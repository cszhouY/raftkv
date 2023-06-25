#ifndef _KVDB_H_
#define _KVDB_H_

#include <map>
#include <string>
#include <vector>


namespace db {

	enum class RESPType {
		SimpleString,
		Error,
		Integer,
		BulkString,
		Array
	};

	class KVDB {
	public:

		std::string commit(const std::string& resp);
	
	private:
		std::map<std::string, std::string> storage;

		std::vector<std::string> parseRESP(const std::string& resp);
	
		RESPType getResponseType(const std::string& token);

		std::vector<std::string> getRESPCommand(std::vector<std::string> & tokens);

		std::string executeRESPCommand(std::vector<std::string> & cmd);
	
	};  // class KVDB

};  // namespace db

#endif