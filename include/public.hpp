#ifndef PUBLIC_H
#define PUBLIC_H
//三件套，idndef define endif ，预编译指令在编译前执行，为了编译器编译时只编译一次这个头文件。
/*
    server和client的公共文件,定义回显给客户端消息的类型
*/

enum EnMsgType{
    LOGIN_MSG = 1,//登录消息
    LOGIN_MSG_ACK = 2,//登录响应消息
    REG_MSG = 3,//注册消息
    REG_MSG_ACK = 4, //注册响应消息
    ONE_CHAT_MSG = 5,//聊天消息
    ADD_FRIEND_MSG = 6,//添加好友消息

    CREATE_GROUP_MSG = 7,//创建群组消息
    ADD_GROUP_MSG = 8,//加入群组消息
    GROUP_CHAT_MSG = 9,//群聊天消息
    
    LOGINOUT_MSG = 10,//注销消息
};

#endif