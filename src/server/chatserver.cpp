#include "chatserver.hpp"
#include<functional>
#include "json.hpp"
#include<string>
#include "chatservice.hpp"

using namespace std;
using json = nlohmann::json;
using namespace placeholders;

//初始化聊天服务器对象
ChatServer::ChatServer(EventLoop* loop,
                const InetAddress &listenAddr,
                const string &nameArg
                )
    : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        //注册链接回调
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

        //注册消息回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

        //设置线程数量
        _server.setThreadNum(4);
    }

//启动服务器
void ChatServer::start()
{
    _server.start();
}
//上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    //客户端断开连接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);//异常管理
        conn->shutdown();//有用户下线，释放socket。
    }
}
//上报消息事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                    Buffer *buffer,
                    Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    
    //通过js里面读出来的["msgid"]，获取一个业务处理器handler（实现绑定的一个方法）
    //目的：是完全解耦网络模块的代码和业务模块的代码，不要在回调函数里面直接调用业务模块的方法。
    //通过js["msg_id"]获取 => 业务handler => conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    //回调消息绑定好的事件处理器，来执行相应的业务处理。
    msgHandler(conn, js, time);
}