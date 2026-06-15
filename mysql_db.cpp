#include "mysql_db.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <map>
#include <thread>

// 数据库配置
static const char* DB_HOST = "127.0.0.1";
static const char* DB_USER = "root";
static const char* DB_PASS = "123";   // 请替换为实际密码
static const char* DB_NAME = "chatroom";
static const int   DB_PORT = 3306;

// 用 map 存储每个线程的 MySQL 连接
static std::mutex mysql_map_mutex;
static std::map<std::thread::id, MYSQL*> mysql_connections;

// 获取当前线程的 MySQL 连接
static MYSQL* GetThreadMySQL() {
    std::thread::id tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(mysql_map_mutex);
    auto it = mysql_connections.find(tid);
    if (it != mysql_connections.end()) {
        return it->second;
    }
    return nullptr;
}

// 设置当前线程的 MySQL 连接
static void SetThreadMySQL(MYSQL* conn) {
    std::thread::id tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(mysql_map_mutex);
    mysql_connections[tid] = conn;
}

// 全局初始化 MySQL 库（主线程调用一次）
bool InitMySQL() {
    if (mysql_library_init(0, nullptr, nullptr)) {
        fprintf(stderr, "[ERROR] mysql_library_init failed\n");
        return false;
    }
    return true;
}

void DestroyMySQL() {
    mysql_library_end();
}

bool InitThreadMySQL() {
    MYSQL* conn = GetThreadMySQL();
    if (conn) return true;  // 已初始化
    
    // 多次尝试初始化
    for (int retry = 0; retry < 3; retry++) {
        conn = mysql_init(nullptr);
        if (conn) break;
        fprintf(stderr, "[WARN] mysql_init attempt %d failed\n", retry + 1);
    }
    
    if (!conn) {
        fprintf(stderr, "[ERROR] mysql_init failed after retries\n");
        return false;
    }
    
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, DB_PORT, nullptr, 0)) {
        fprintf(stderr, "[ERROR] MySQL connect failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return false;
    }
    
    // 设置字符集
    mysql_set_character_set(conn, "utf8");
    
    // 保存到 map 中
    SetThreadMySQL(conn);
    
    printf("[INFO] Thread MySQL connected successfully\n");
    return true;
}

void DestroyThreadMySQL() {
    MYSQL* conn = GetThreadMySQL();
    if (conn) {
        mysql_close(conn);
        SetThreadMySQL(nullptr);
    }
}

// 执行无结果集的SQL
static bool ExecSQL(const char* sql) {
    MYSQL* conn = GetThreadMySQL();
    if (!conn) {
        fprintf(stderr, "[ERROR] No MySQL connection for current thread\n");
        return false;
    }
    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "SQL error: %s\n", mysql_error(conn));
        return false;
    }
    return true;
}

bool RegisterUser(const std::string& username, const std::string& password, std::string& errmsg) {
    MYSQL* conn = GetThreadMySQL();
    if (!conn) {
        errmsg = "Database connection error";
        return false;
    }
    
    // 检查用户名是否存在
    char query[256];
    snprintf(query, sizeof(query), "SELECT id FROM users WHERE username='%s'", username.c_str());
    if (mysql_query(conn, query) != 0) {
        errmsg = mysql_error(conn);
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (res->row_count > 0) {
        mysql_free_result(res);
        errmsg = "Username already exists";
        return false;
    }
    mysql_free_result(res);

    // 插入用户，默认昵称与用户名相同
    snprintf(query, sizeof(query),
             "INSERT INTO users (username, password, nickname) VALUES ('%s', '%s', '%s')",
             username.c_str(), password.c_str(), username.c_str());
    if (!ExecSQL(query)) {
        errmsg = mysql_error(conn);
        return false;
    }
    return true;
}

bool LoginUser(const std::string& username, const std::string& password, std::string& errmsg, std::string& nickname) {
    MYSQL* conn = GetThreadMySQL();
    if (!conn) {
        errmsg = "Database connection error";
        return false;
    }
    
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT nickname FROM users WHERE username='%s' AND password='%s'",
             username.c_str(), password.c_str());
    if (mysql_query(conn, query) != 0) {
        errmsg = mysql_error(conn);
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (res->row_count == 0) {
        mysql_free_result(res);
        errmsg = "Invalid username or password";
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    nickname = row[0] ? row[0] : "";
    mysql_free_result(res);
    return true;
}

bool ChangeNickname(const std::string& username, const std::string& newnick, std::string& errmsg) {
    char query[256];
    snprintf(query, sizeof(query), "UPDATE users SET nickname='%s' WHERE username='%s'",
             newnick.c_str(), username.c_str());
    if (!ExecSQL(query)) {
        MYSQL* conn = GetThreadMySQL();
        errmsg = conn ? mysql_error(conn) : "Database error";
        return false;
    }
    return true;
}

bool DeleteAccount(const std::string& username, std::string& errmsg) {
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM users WHERE username='%s'", username.c_str());
    if (!ExecSQL(query)) {
        MYSQL* conn = GetThreadMySQL();
        errmsg = conn ? mysql_error(conn) : "Database error";
        return false;
    }
    return true;
}

bool SetUserOnline(const std::string& username, bool online) {
    char query[256];
    snprintf(query, sizeof(query), "UPDATE users SET online=%d WHERE username='%s'",
             online ? 1 : 0, username.c_str());
    return ExecSQL(query);
}