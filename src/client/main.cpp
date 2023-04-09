#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <map>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录用户当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 接收线程(发送数据和接收数据是并行的，所以使用主线程来发送数据，这个线程来接收数据)
void readTaskHandler(int clientfd);
// 获取系统事件(聊天时需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu();

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:addfriendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端处理命令
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

    return std::string(date);
}
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}

//"loginout" command handler
void loginout(int clientfd, string)
{

    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

//"help" command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

//"addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

//"chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    // cout<< "niubi1"<<endl;
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    // cout<< "niubi2"<<endl;
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    // cout<< "niubi3"<<endl;
    js["id"] = g_currentUser.getId();
    // cout<< "niubi4"<<endl;
    js["name"] = g_currentUser.getName();
    // cout<< "niubi5"<<endl;
    js["toid"] = friendid;
    // cout<< "niubi6"<<endl;
    js["msg"] = message;
    // cout<< "niubi7"<<endl;
    js["time"] = getCurrentTime();
    // cout<< "niubi8"<<endl;
    string buffer = js.dump();
    // cout<< "niubi9"<<endl;

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    cout << "niubbibi" << endl;
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;                 // 存储命令
        int idx = commandbuf.find(":"); // 在命令里面找冒号
        if (-1 == idx)                  // 没招到冒号，命令就是输入的
        {
            command = commandbuf;
        }
        else // 找到冒号，就把第一个冒号前面的命令记录下来
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command); // 在命令map里面找对应函数
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};                   // 接收数据，显示数据
        int len = recv(clientfd, buffer, 1024, 0); // 当前用户登出后，这个线程会阻塞再这里
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收chatserver转发的数据，反序列化生成的json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said: " << js["msg"].get<string>() << endl;
            continue;
        }
        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息 ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接，如果连接失败就退出
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单、登录、注册、退出
        cout << "===============================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "===============================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车，因为你输入了一个整数再按一下回车，cin会把回车也都进去

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv login reponse error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 登录失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();
                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表信息
                        if (responsejs.contains("groups"))
                        {

                            // 初始化，清空所在群组信息
                            g_currentUserGroupList.clear();

                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }

                        // 显示登录用户的基本信息
                        showCurrentUserData();
                        // 显示当前用户的离线消息， 个人聊天消息或者群组消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);

                                int msgtype = js["msgid"].get<int>();
                                // 将离线消息分为群消息和1对1消息进行展示
                                if (ONE_CHAT_MSG == msgtype)
                                {
                                    cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                                         << "said: " << js["msg"].get<string>() << endl;
                                }
                                if (GROUP_CHAT_MSG == msgtype)
                                {
                                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                                         << "said: " << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        // 登录成功，启动接收线程负责接收数据,只启动一次，读线程只需要一个就好。
                        static int threadnumber = 0;
                        if (threadnumber == 0)
                        {

                            
                            std::thread readTask(readTaskHandler, clientfd); // 底层调用pthread_create
                            readTask.detach();                               // pthread_detach设置分离线程，子线程运行完，PCB自动回收，防止内存泄漏
                            threadnumber++;
                        }

                        isMainMenuRunning = true;
                        // 进入聊天主菜单页面
                        mainMenu(clientfd);
                    }
                }
            }
             break;
        }
        case 2: // 注册业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50); // 因为cin 或者scanf会把空格跳过，默认遇到回车才结束
            cout << "userpassword:";
            cin.getline(pwd, 50);
            json js;
            js["msgid"] = REG_MSG; // 注册消息
            js["name"] = name;     // 用户名
            js["password"] = pwd;
            string request = js.dump(); // json对象序列化成字符串形式发送

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error: " << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0); // 阻塞等待，注册请求的响应
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else // 响应成功
                {
                    json responsejs = json::parse(buffer);   // 反序列化生成json数据
                    if (0 != responsejs["errno"].get<int>()) // errno = 1，注册失败
                    {
                        cerr << name << "is already exist, register error!" << endl;
                    }
                    else // 注册成功，得到一个id号码，登录时就用这个号码
                    {
                        cout << name << "register success, userid is " << responsejs["id"]
                             << ", do not forget it!" << endl;
                    }
                }
            }
            continue;
        }
            

        case 3: // 退出业务
        {
            close(clientfd);
            exit(0);
        }
        default: // 输出其他的重新循环
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}
void showCurrentUserData()
{
    cout << "============login user==================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << "name:" << g_currentUser.getName() << endl;
    cout << "----------------------------friend list-----------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------------group list---------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "=================================================" << endl;
}
