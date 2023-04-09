#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

//user表的数据操作类
class UserModel{
public:
    //User表的增加方法
    bool insert(User &user);

    //根据用户号码查询用户信息, 也可以设置成指针，可以表示空指针，表示没查找
    User query(int id);

    //更新用户的状态信息
    bool updateState(User user);

    //重置用户的状态信息
    void resetState();
};


#endif