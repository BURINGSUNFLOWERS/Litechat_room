#include<iostream>
#include<string>
#include<thread>
#include<mutex>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<cstring>

// 防止打印混乱
std::mutex cout_mutex;
int sockfd;
bool running=true;

// 接收线程
void recvThread(){
    char buf[1024];
    while(running){
        memset(buf,0,sizeof(buf));
        ssize_t n=recv(sockfd,buf,sizeof(buf)-1,0);
        if(n<=0){
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout<<"\n[Disconnceted from server]\n";
            running=false;
            break;
        }
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout<<buf;
        std::cout.flush();
    }
}
int main(int argc,char* argv[]){
    if(argc!=3){
        std::cerr<<"Usage"<<argv[0]<<"<server_ip> <port>\n";
        return  1;
    }
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        perror("scoket");
        return 1;
    }
    sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(atoi(argv[2]));
    if(inet_pton(AF_INET,argv[1],&serv_addr.sin_addr)<=0){
        std::cerr<<"Invalid address\n";
        return 1;
    }
    if(connect(sockfd,(sockaddr*)&serv_addr,sizeof(serv_addr))<0){
        perror("connect");
        return 1;
    }
    std::cout<<"Connected to server. Commands:\n"
              << "  REGISTER <user> <pass>\n"
              << "  LOGIN <user> <pass>\n"
              << "  MSG <message>\n"
              << "  CHNICK <newnick>\n"
              << "  DELETEACCOUNT\n"
              << "  QUIT\n";
    std::thread recv(recvThread);
    recv.detach();

    std::string line;
    while(running&&std::getline(std::cin,line)){
        if(line.empty()) continue;
        line+="\n";
        send(sockfd,line.c_str(),line.size(),0);
        if(line=="QUIT\n"||line=="DELETEACCOUNT\n"){
              // 等待一会儿接收响应
              sleep(1);
              break;
        }
    }
    running=false;
    close(sockfd);
     return 0;
}