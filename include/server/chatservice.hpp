#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include<muduo/net/TcpConnection.h>
#include<unordered_map>
#include<functional>
#include<mutex>

using namespace std;
using namespace muduo;
using namespace muduo::net;
#include"json.hpp"
using json = nlohmann::json;

#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"

#include "groupmodel.hpp"
#include "redis.hpp"


//消息ID对应事件的回调
//表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;


/*
    单例模式设计聊天服务器业务类
*/
class ChatService
{
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //1v1聊天服务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

    //服务器异常，业务重置方法
    void reset();

    //创建群组
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //加入群组
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //redis获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);


private:
    ChatService();//构造函数私有化

    //存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;//使用unordered_map存储消息处理的一个表。

    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    //定义互斥锁，保证_userconnMap的线程安全
    mutex _connMutex;

    //数据操作类对象
    UserModel _userModel;//对用户表的操作
    OfflineMsgModel _offlineMsgModel;//对离线消息表处理的操作
    FriendModel _friendModel;//对好友关系表的操作
    GroupModel _groupModel;//对群组表的操作

    Redis _redis;//redis操作对象

};

#endif