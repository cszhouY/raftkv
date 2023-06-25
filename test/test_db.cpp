#include <string>
#include <iostream>

#include "KVDB.h"

int main() {
	db::KVDB kvdb;
	std::string resp = "*4\r\n$3\r\nSET\r\n$7\r\nCS06142\r\n$5\r\nCloud\r\n$9\r\nComputing\r\n";
	std::cout << kvdb.commit(resp) << std::endl;
}