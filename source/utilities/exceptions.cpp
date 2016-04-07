#include <exceptions.hpp>

#include <exception>
#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#else
#include <cstring>
#include <cerrno>
#endif

using namespace utilities;
using namespace std;

base_exception::base_exception(const string& str) noexcept : msg(str) {}
base_exception::base_exception(string&& str) noexcept : msg(str) {}

const char* base_exception::what() const noexcept {
	return msg.c_str();
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
base_exception::base_exception(DWORD code) noexcept {

	char *s = NULL;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL, code,
	               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				   (LPTSTR)&s, 0, NULL);
	msg = string(s);
	LocalFree(s);
}
#else
base_exception::base_exception(int code) noexcept {
	msg = string(strerror(code));
}
#endif


socket_exception::socket_exception() noexcept {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

	char *s = NULL;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL, WSAGetLastError(),
	               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				   (LPTSTR)&s, 0, NULL);
	msg = string(s);
	LocalFree(s);

#else
	msg = string(strerror(errno));
#endif
}

db_exception::db_exception(int err) noexcept : base_exception(sqlite3_errstr(err)){
}

db_exception::db_exception(sqlite3* db) noexcept : base_exception(sqlite3_errmsg(db)){
}

db_exception::db_exception(char* err) noexcept : base_exception(err){
	sqlite3_free(err);
}


