#include "chatservice.hpp"
#include "public.hpp"
#include <string>
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>
using namespace std;
using namespace muduo;
// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组相关业务相关事件处理回调
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // redis连接服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调,2各参数：发生消息的通道号和具体的消息
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online用户的状态，设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid" << msgid << "can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "DO LOG SERVICE";
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getPwd() == pwd && user.getId() == id)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errormsg"] = "该账号已经登录，请重新输入新的账号";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息,因为每一个登录用户都要维护这样一个连接，所以这就设计到
            // 往这个连接队列里面添加连接的操作，这就涉及到了线程安全的问题，因为C++STL设计的时候
            // 都没有考虑线程安全问题，所以需要对这个队列添加一个互斥锁。用C++11再带的
            // 一切都是自动释放资源，不要自己手动释放，容易忘记。
            {
                lock_guard<mutex> lock(_connMutex); // 定义一个锁，对connMutx加锁，
                                                    // 作用域就在这个大括号范围内
                _userConnMap.insert({id, conn});
            }

            // 用户id登录成功后，需要向redis订阅一个通道，可以直接用id号作为通道号
            _redis.subscribe(id);

            // 登录成功，更新用户状态信息，state offline => online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息。
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息之后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 查询该用户的好友信息，并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组消息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    else
    {

        // 登录失败,该用户不存在，或用户存在但是密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errormsg"] = "用户名或者密码错误";
        conn->send(response.dump());
    }
}
// 处理注册业务  填一个name 和一个 password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "DO REG SERVICE";
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK; // 响应注册消息
        response["errno"] = 0;           // 表示响应成功
        response["id"] = user.getId();
        conn->send(response.dump()); // 封装完毕发回去
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK; // 响应注册消息
        response["errno"] = 1;           // 表示响应失败
        conn->send(response.dump());     // 封装完毕发回去
    }
}
// 处理客户端异常退出，做两个操作，1是把连接从连接表里面删除，2是把用户状态从online改为offline
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    { // 锁的作用域，一出这个作用域就释放锁了，对连接表的操作需要保证线程安全
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户异常退出也要取消一下redis的订阅
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 1v1聊天服务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>(); // 得到目标用户的连接id
    cout << "wocao niubi!!!" << endl;
    bool userState = false; // 标识该用户是否在线

    {
        lock_guard<mutex> lock(_connMutex); // 对于连接的操作都要线程安全
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) // 查找用户是否在线，加锁查找
        {
            userState = true;            // 用户在线，转发消息,服务器主动推动消息给toid用户
            it->second->send(js.dump()); // 服务器做一个转发消息的功能，使用目标用户的连接给它发送源用户的这条消息
            return;
        }
    }

    // 查询toid是否在线（看看是不是在不同服务器上连接着）
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        // 如果在其他服务器连接着，那就向该用户对应的通道发送消息
        _redis.publish(toid, js.dump());
        return;
    }

    // 用户不在线
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务 msg id friend
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid); // 新增好友关系，双向好友，只需要添加一行即可
    _friendModel.insert(friendid, userid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    // 查找当前用户所在群组的其他用户状态
    lock_guard<mutex> lock(_connMutex); // 在对在线用户进行查找的时候，需要保证线程安全，加锁
    for (int id : useridVec)
    {

        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线（看看是不是在不同服务器上连接着）
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                // 如果在其他服务器连接着，那就向该用户对应的通道发送消息
                _redis.publish(id, js.dump());
                // return;
            }

            // 存储离线消息
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}
// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户id下线，响应的取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// redis获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
   // json js = json::parse(msg.c_str());
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    //存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}