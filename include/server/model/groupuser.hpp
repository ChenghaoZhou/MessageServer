#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.hpp"

//群组用户，多了一个role角色信息，从user类直接继承，复用user的其他信息
class GroupUser : public User
{
    
public:
    void setRole(string role)   { this->role = role;}
    string getRole()    {return this->role;}

private:
    string role;
};


#endif