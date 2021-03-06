#ifndef SOURCE_SERVER_INCLUDE_DATABASE_HPP_
#define SOURCE_SERVER_INCLUDE_DATABASE_HPP_

#include <settings.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <sqlite3.h>

namespace std {
template<>
struct default_delete<sqlite3> {
	constexpr default_delete() = default;
	inline void operator()(sqlite3*p) const {
		sqlite3_close(p);
	}
};

template<>
struct default_delete<sqlite3_stmt> {
	constexpr default_delete() = default;
	inline void operator()(sqlite3_stmt*p) const {
		sqlite3_finalize(p);
	}
};
}

namespace server {

#define SQL_INIT "PRAGMA foreign_keys = ON;PRAGMA journal_mode=WAL;PRAGMA recursive_triggers = true;"
#define SQL_AUTH "SELECT password FROM users WHERE username=?1"
#define SQL_CREATE "INSERT INTO files (username, path, time_stamp, file_id) VALUES (?1,?2,?3,?4)"
#define SQL_CHMOD "UPDATE files SET time_stamp=?3, mod=?4 WHERE username=?1 AND path=?2"
#define SQL_EXISTS "SELECT file_id FROM files WHERE username=?1 AND path=?2 UNION "\
					"SELECT file_id FROM history WHERE username=?1 AND path=?2"
#define SQL_WRITE "INSERT INTO dispatch_write (username, path, time_stamp, file_id) VALUES (?1, ?2, ?3, ?4)"
#define SQL_MOVE "INSERT INTO GROUP_CHANGES (T, username, path, time_stamp, new_path) VALUES ('m', ?1, ?2, ?3, ?4)"
#define SQL_APPLY "UPDATE users SET lastSync=?2 WHERE username = ?1;"
#define SQL_DELETE "INSERT INTO GROUP_CHANGES (T, username, path, time_stamp) VALUES ('d', ?1, ?2, ?3)"
#define SQL_SYNC "SELECT path, time_stamp FROM files WHERE username=?1"
#define SQL_VERSION "SELECT file_id FROM history WHERE username=?1 AND path=?2 AND time_stamp=?3 UNION "\
					"SELECT file_id FROM files WHERE username=?1 AND path=?2 AND time_stamp=?3"
#define SQL_LIST_V "SELECT time_stamp FROM history WHERE username=?1 AND path=?2 ORDER BY time_stamp DESC"

class database;

class user_context {
	database& db;

	friend class database;
	user_context(std::string&, std::string&, std::string&, database&);
public:
	const std::string& usr, &pass;
	std::string path;

	bool auth();
	void create(int64_t);
	bool version_exists(std::string&);
	void write(int64_t, std::string&);
	void chmod(int64_t, uint32_t);
	void move(int64_t, std::string&);
	void apply(int64_t);
	void remove(int64_t);
	std::vector<std::pair<std::string,int64_t>> sync();
	std::string version(int64_t);
	std::vector<int64_t> versions();
};

class database {
	std::unique_ptr<sqlite3> connection;
	std::unique_ptr<sqlite3_stmt> auth, create,
			chmod, write, move, remove, version, list, sync, version_exists, apply;
	std::mutex busy;
	friend class user_context;
public:
	database(const char*db_name = settings::inst().db_name.value.c_str());
	user_context getUserContext(std::string&, std::string&, std::string&);
};

}

#endif /* SOURCE_SERVER_INCLUDE_DATABASE_HPP_ */
