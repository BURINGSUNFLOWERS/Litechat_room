#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <sstream>
#include"common.h"
#include"threadpool.h"
#include"mysql_db.h"
#include <algorithm> 

#define MAX_EVENTS 1024
#define LISTEN_PORT 8888
#define THREAD_POOL_SIZE 4

std::map<int, std::shared_ptr<ClientInfo>> clients;
std::mutex clients_mutex;

ThreadPool*pool=nullptr;

int setNonBlocking(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    return fcntl(fd,F_SETFL ,flags | O_NONBLOCK);

}

void safeSend(int fd, const std::string& msg ){

    std::shared_ptr<ClientInfo> client;
    {

        std:: lock_guard<std::mutex> lock(clients_mutex);
        auto it=clients.find(fd);
        if(it!=clients.end()){
            client=it->second;
        }
    }
    if(!client)  return ;
    std::lock_guard<std::mutex> lock(client->send_mutex);
    const char* data =msg.data();
    size_t len=msg.size();
    while(len>0){
        ssize_t n=send(fd,data,len,0);
        if(n<=0) break;
        data+=n;
        len-=n;
    }
}

void broadcast(const std::string& msg,int exclude_fd=-1){
    std::vector<std::shared_ptr<ClientInfo > >snapshot;
    {
        std::lock_guard<std:: mutex> lock(clients_mutex);
        for(auto& pair:clients){
            if(pair.first!=exclude_fd)
                snapshot.push_back(pair.second);

            
        }
        
    }
    for(auto& cli :snapshot){
            std::lock_guard<std::mutex> lock (cli->send_mutex);
            const char* data=msg.data();
            size_t len=msg.size();
            while (len>0)
            {
                ssize_t n=send(cli->fd,data,len,0);
                if(n<=0) break;
                data+=n;
                len-=n;
            }
            
        }
}
void handleDisconnect (int fd){
    std::shared_ptr<ClientInfo> client;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it=clients.find(fd);
        if(it!=clients.end()){
            client =it->second;
            clients.erase(it);
            
        }

    }
    if(!client) return;
    close(fd);
    if(client->logged_in){
        std::string username=client->username;
        pool->Enqueue([username]{
            SetUserOnline(username,false);

        });
        std::string offline_msg="USER_OFFLINE"+username+"\n";

        broadcast(offline_msg);
        std::cout<<"[INFO] User offline:"<<username<<std::endl;

    }
}

void processCommand(std::shared_ptr<ClientInfo> client,const std:: string& cmd){
   // 清理 \r 字符
    std::string clean_cmd = cmd;
    clean_cmd.erase(std::remove(clean_cmd.begin(), clean_cmd.end(), '\r'), clean_cmd.end());
    
    std::cout << "[DEBUG] Clean command: '" << clean_cmd << "'" << std::endl;
    
    std::istringstream iss(clean_cmd);
    std::string type;
    if (!(iss >> type)) {
        std::cout << "[DEBUG] Failed to extract command type" << std::endl;
        return;
    }
    
    std::cout << "[DEBUG] Command type: '" << type << "'" << std::endl;
    
    // 添加字符串长度输出，用于调试
    std::cout << "[DEBUG] type length: " << type.length() 
              << ", compare LOGIN: " << (type == "LOGIN") 
              << ", compare REGISTER: " << (type == "REGISTER") << std::endl;

    if (type == "REGISTER") {
        std::cout << "[DEBUG] Entering REGISTER branch" << std::endl;
        std::string username, password;
        if (!(iss >> username >> password)) {
            safeSend(client->fd, "REGISTER_FAIL Invalid arguments\n");
            return;
        }
        std::string errmsg;
        bool ok = RegisterUser(username, password, errmsg);
        if (ok) {
            safeSend(client->fd, "REGISTER_SUCCESS\n");
        } else {
            safeSend(client->fd, "REGISTER_FAIL " + errmsg + "\n");
        }
    }
    else if (type == "LOGIN") {
        std::cout << "[DEBUG] Entering LOGIN branch" << std::endl;
        if (client->logged_in) {
            safeSend(client->fd, "LOGIN_FAIL Already logged in\n");
            return;
        }
        std::string username, password;
        if (!(iss >> username >> password)) {
            safeSend(client->fd, "LOGIN_FAIL Invalid arguments\n");
            return;
        }
        std::string errmsg, nickname;
        bool ok = LoginUser(username, password, errmsg, nickname);
        if (!ok) {
            safeSend(client->fd, "LOGIN_FAIL " + errmsg + "\n");
            return;
        }
        client->username = username;
        client->nickname = nickname.empty() ? username : nickname;
        client->logged_in = true;
        SetUserOnline(username, true);
        safeSend(client->fd, "LOGIN_SUCCESS\n");
        std::string online_msg = "USER_ONLINE " + username + "\n";
        broadcast(online_msg, client->fd);
        std::cout << "[INFO] User login: " << username << std::endl;
    }
    else if(type=="MSG"){
        if(!client->logged_in){
            safeSend(client->fd,"ERROR Not logged in\n");
            return;
        }
        std::string message;
         std::getline(iss,message);
        if(!message.empty()&&message[0]==' ' ) message.erase(0,1);
        if(message.empty()) return;
        std::string full_msg="MSG"+client->nickname+":"+message+"\n";
        broadcast(full_msg);
    }
    else if (type == "CHNICK") {
        if (!client->logged_in) {
            safeSend(client->fd, "ERROR Not logged in\n");
            return;
        }
        std::string newnick;
        if (!(iss >> newnick)) {
            safeSend(client->fd, "CHNICK_FAIL Invalid arguments\n");
            return;
        }
        std::string errmsg;
        if (ChangeNickname(client->username, newnick, errmsg)) {
            client->nickname = newnick;
            safeSend(client->fd, "CHNICK_SUCCESS " + newnick + "\n");
        } else {
            safeSend(client->fd, "CHNICK_FAIL " + errmsg + "\n");
        }
    }
    else if(type=="DELETEACCOUNT"){
        if(!client->logged_in){
            safeSend(client->fd,"ERROR Not logged in\n");
            return;
        }
            std::string errmsg;
        if(DeleteAccount(client->username,errmsg)){
            safeSend(client-> fd,"ELETEACCOUNT_SUCCESS\n");
            std::string username=client->username;

            std::string offline_msg="USER_OFFLINE"+username+"\n";
            broadcast(offline_msg);

            client->logged_in=false;
         close(client->fd);

            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.erase(client->fd);
         }
            std::cout<<"[INFO] Account deleted, user offline:"<<username<<std::endl;
     }
        else{
            safeSend(client->fd,"DELETEACCOUNT_FAIL " + errmsg + "\n");
        }
    
    }
    else if(type=="QUIT"){
       if (client->logged_in) {
            std::string username = client->username;
            SetUserOnline(username, false);
            std::string offline_msg = "USER_OFFLINE " + username + "\n";
            broadcast(offline_msg);
        }
        safeSend(client->fd,"BYE\n");
        close(client->fd);
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(client->fd);

        }
       
    }
     else{
        safeSend(client->fd,"ERROR Unknown command\n");
    }
} 
    std::vector<std::string> extractComands(std::string& buf){
        std::vector<std::string> cmds;
        size_t pos=0;
        while((pos=buf.find('\n'))!=std::string::npos){
            cmds.push_back(buf.substr(0,pos));
            buf.erase(0,pos+1);
        }
        return cmds;
}
int main(){

    ThreadPool threadPool(THREAD_POOL_SIZE);
    pool=&threadPool;
    
    int listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd<0){
        perror("socket");
        return 1;
    }
    // 地址复用
    int optval=1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR, &optval,sizeof(optval));

    sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(LISTEN_PORT);
    if(bind(listen_fd,(sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("bind");
        return 1;
    }
    if(listen(listen_fd,SOMAXCONN)<0){
        perror("listen");
        return 1;
    }
    setNonBlocking(listen_fd);
 // epoll 创建
    int epfd=epoll_create1(0);
    if(epfd<0){
        perror("epoll_create1");
        return 1;
    }

    epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=listen_fd;
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,listen_fd,&ev)<0){
        perror("epoll_ctr listen");
        return 1;
    }
    epoll_event events[MAX_EVENTS];
    std::cout<<"Chat server started on port"<<LISTEN_PORT<<std::endl;
    while(true){
        int nfds=epoll_wait(epfd,events,MAX_EVENTS,-1);
        if(nfds<0){
            perror("epoll_wait");
            break;
        }
       for(int i=0;i<nfds;i++){
        int fd=events[i].data.fd;
        // 新连接
        if(fd==listen_fd){
            while(true){
                sockaddr_in client_addr;
                socklen_t client_len=sizeof(client_addr);
                int conn_fd=accept(listen_fd,(sockaddr*)&client_addr,&client_len);
                if(conn_fd<0){
                    if(errno==EAGAIN||errno==EWOULDBLOCK) break;
                    perror("accept");
                    break;
                }
                setNonBlocking(conn_fd);

                // 加入epoll
                ev.events=EPOLLIN|EPOLLRDHUP;
                ev.data.fd=conn_fd;
                if(epoll_ctl(epfd,EPOLL_CTL_ADD,conn_fd,&ev)<0){
                    perror("epoll_ctr add client");
                    close(conn_fd);
                    continue;
                }
                auto client_ptr=std::make_shared<ClientInfo>(conn_fd);
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    clients[conn_fd]=client_ptr;
                }
                std::cout<<"[INFO] New connection fd=" <<conn_fd<<std::endl;

            }
            
        }
        else{
            // 客户端数据或关闭事件
                auto it =clients.find(fd);
                if(it==clients.end()){
                    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                    close(fd);
                    continue;

                }
                auto client=it->second;
                if(events[i].events &(EPOLLRDHUP|EPOLLHUP|EPOLLERR)){
                    // 对端关闭或出错
                    handleDisconnect(fd);
                    continue;
                }
                if(events[i].events & EPOLLIN){
                    // 读取数据
                    char buf[1024];
                    while(true){
                        ssize_t n=recv(fd,buf,sizeof(buf),0);
                        if(n>0){
                            client->inbuf.append(buf,n);
                            
                        }
                        else if(n==0){
                            // 对端关闭
                            handleDisconnect(fd);
                            goto next_event;
                        }
                        else{
                            if(errno==EAGAIN || errno==EWOULDBLOCK) break;
                                handleDisconnect(fd);
                                goto next_event;
                        }
                    }
                    // 提取完整命令
                    auto cmds=extractComands(client->inbuf);
                    for(const auto& cmd:cmds){
                        if(cmd.empty()) continue;
                        // 将命令处理提交到线程池
                        // shared_ptr 捕获，确保生命周期
                        auto client_copy=client;
                        pool->Enqueue([client_copy,cmd]{
                            processCommand(client_copy,cmd);
                        });

                    }
                }
            }
            next_event: ;
       }
    }
    close(epfd);
    close(listen_fd);
    return 0;
}