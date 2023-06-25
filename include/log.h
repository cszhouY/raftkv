#ifndef _LOG_H_
#define _LOG_H_

#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <ctime>

// #define _LOG_OFF_

template <typename T>
void osspack(std::ostringstream &o, T &&t) {
	o << t;
}

template <typename T, typename... Args>
void osspack(std::ostringstream &o, T &&t, Args &&...args) {
    o << t;
    osspack(o, std::forward<Args>(args)...);
}

template <typename... Args>
void print(const std::string & level, Args &&...args) {
	std::ostringstream o;
	time_t now = time(0);
	char* dt = ctime(&now);
	dt[strlen(dt) - 1] = '\0';
	osspack(o, "[", level, "] [", dt, "] ", std::forward<Args>(args)...);
	o << "\n";
	std::cout << o.str();
}

template <typename... Args>
void log_info(Args &&... args) {
	print("INFO", std::forward<Args>(args)...);
}

template <typename... Args>
void log_debug(Args &&... args) {
	print("DEBUG", std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(Args &&... args) {
	print("WARN", std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(Args &&... args) {
	print("ERROR", std::forward<Args>(args)...);
}

#ifndef _LOG_OFF_
#define INFO(...) log_info(__VA_ARGS__)
#define DEBUG(...) log_debug(__VA_ARGS__)
#define WARN(...) log_warning(__VA_ARGS__)
#define ERROR(...) log_error(__VA_ARGS__)

#else
#define INFO(...) 
#define DEBUG(...) 
#define WARN(...)
#define ERROR(...)
#endif

#endif  // _LOG_H_