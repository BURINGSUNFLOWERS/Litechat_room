#ifndef MYSQL_DB_H
#define MYSQL_DB_H

#include<mysql/mysql.h>
#include<string>

bool InitThreadMySQL();
void DestroyThreadMySQL();

bool RegisterUser(const std::string& username,const std::string& password,std:: string& errmsg);
bool LoginUser(const std::string& username,const std::string& password,std:: string& errmsg ,std::string& nickname);
bool ChangeNickname(const std::string& usrname,const std::string& newnick,std::string& errmsg);
bool DeleteAccount(const std::string& username,std::string& errmsg);
bool SetUserOnline(const std::string& username,bool online);

#endif