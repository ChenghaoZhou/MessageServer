#include "friendmodel.hpp"
#include "db.h"

// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend values(%d, %d)", userid, friendid);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 返回用户好友列表  friendid，，做个两个表的联合查询即可。
vector<User> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    //sprintf(sql, "select a.id, a.name, a.state from User a inner join Friend b on (b.friendid = a.id and b.userid != a.id) or (b.userid = a.id and b.friendid != a.id) where b.userid = %d or b.friendid = %d", userid, userid);
    sprintf(sql, "select a.id, a.name, a.state from User a inner join Friend b on b.friendid = a.id where b.userid = %d", userid);
    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有好友放入vec中返回
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                //user.set(atoi(row[3]));
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}