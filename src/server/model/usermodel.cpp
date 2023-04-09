#include "usermodel.hpp"
#include "db.h"
#include <iostream>
using namespace std;

// user表的增加方法
bool UserModel::insert(User &user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getconnection()));
            return true;
        }
    }
    return false;
}

// 根据用户号码查询用户信息
User UserModel::query(int id)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d", id);

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr) // 如果查到了
        {
            MYSQL_ROW row = mysql_fetch_row(res); // 返回一行
            if (row != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);

                mysql_free_result(res); // 因为是指针结果，所以需要释放掉，否则会造成内存泄漏
                return user;
            }
        }
    }
    return User(); // 没找到，返回默认
}

// 更新用户状态信息
bool UserModel::updateState(User user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "update User set state = '%s' where id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }
    return false; // 更新失败
}
// 重置用户状态的信息
void UserModel::resetState()
{
    char sql[1024] = {0};
    sprintf(sql, "update User set state = 'offline' where state = 'online'");
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
