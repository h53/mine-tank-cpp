#include <stdio.h>
#include <map>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sstream>
#include <signal.h>

#define ERROR_EXIT(m)       \
    do                      \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    } while (0)

#define BUFFER_SIZE 1024

typedef std::vector<struct epoll_event> EventList;
class ClientState;
static std::map<int, ClientState *> clientStates;
void handle_sigpipe(int sig)
{
    printf("recv a sig = %d\n", sig);
}

class ByteArray
{
public:
    int initSize = 0;
    int capacity = 0;
    char *bytes;
    int readIdx = 0;
    int writeIdx = 0;

    inline int Length()
    {
        //printf("length = %d\n",writeIdx - readIdx);
        return writeIdx - readIdx;
    }

    inline int Remain()
    {
        return capacity - writeIdx;
    }

    ByteArray(int size = BUFFER_SIZE) : capacity(size),
                                        initSize(size),
                                        readIdx(0),
                                        writeIdx(0)
    {
        bytes = new char[BUFFER_SIZE]{0};
    }

    ~ByteArray(){
        std::cout << "free bytes[]" << std::endl;
        delete[] bytes;
        std::cout << "free bytes[] done!" << std::endl;
    }

    inline char *WriteAddr()
    {
        return bytes + writeIdx;
    }

    inline char *ReadAddr()
    {
        return bytes + readIdx;
    }

    void CheckAndMoveBytes()
    {
        printf("check bytes -----\n");
        if (this->Length() < 8)
        {
            printf("check yes +++++\n");
            MoveBytes();
        }
    }

    void MoveBytes()
    {
        printf("MoveBytes, readIdx = %d ", readIdx);
        printf("writeIdx = %d ", writeIdx);
        printf("length = %d ", this->Length());
        printf("remain = %d ", this->Remain());
        printf("size = %d\n", capacity);

        memcpy(bytes, bytes + readIdx, this->Length() * sizeof(char));
        writeIdx = this->Length();
        readIdx = 0;

        printf("+++++++++, readIdx = %d ", readIdx);
        printf("writeIdx = %d ", writeIdx);
        printf("length = %d ", this->Length());
        printf("remain = %d ", this->Remain());
        printf("size = %d\n", capacity);
    }

    size_t BodyLength()
    {
        printf("bodylength %d\n", bytes[readIdx] | bytes[readIdx + 1] << 8);
        return bytes[readIdx] | bytes[readIdx + 1] << 8;
    }
};

class ClientState
{
public:
    std::string socketStr;
    ByteArray *recvbuf;
    float posX;
    float posY;
    short dirX;
    short dirY;

    ClientState() : posX(0), posY(0), dirX(0), dirY(0)
    {
        recvbuf = new ByteArray();
    }

    ~ClientState(){
        std::cout << "free recvbuf " << std::endl;
        delete recvbuf;
        std::cout << "free recvbuf done!" << std::endl;
    }
};

void split(const std::string &s, std::vector<std::string> &tokens, char delim = ' ')
{
    tokens.clear();
    auto string_find_first_not = [s, delim](size_t pos = 0) -> size_t {
        for (size_t i = pos; i < s.size(); i++)
        {
            if (s[i] != delim)
                return i;
        }
        return std::string::npos;
    };
    size_t lastPos = string_find_first_not(0);
    size_t pos = s.find(delim, lastPos);
    while (lastPos != std::string::npos)
    {
        tokens.emplace_back(s.substr(lastPos, pos - lastPos));
        lastPos = string_find_first_not(pos);
        pos = s.find(delim, lastPos);
    }
}

void SendStr(int connfd, std::string str)
{
    uint16_t *len = new uint16_t(str.size());
    const char * lenByte = (char *)len;
    char *sendByte = new char[*len + 2]{0};
    memcpy(sendByte,lenByte,sizeof(*len));
    memcpy(sendByte + 2, str.c_str(), *len);
    std::cout << "str size " << *len << std::endl;
    //std::cout << "len size " << sizeof(len) << std::endl;

    size_t nleft = *len + 2;
    std::cout << "need to send " << nleft << std::endl;
    int nsend = 0;
    while (nleft > 0)
    {
        //std::cout << "sending " << nleft << "bytes" << std::endl;
        if (nsend = (write(connfd, sendByte + nsend, nleft)) < 0)
        {
            if (errno == EINTR)
                continue;
            ERROR_EXIT("send");
        }
        else if (nsend == 0)
            //continue;
            break;
        nleft -= nsend;
        std::cout << "sended " << nsend << " left " << nleft << std::endl;
    }
    std::cout << "send complete" << std::endl;
    delete[] sendByte;
    delete len;
}

void BroadCast(std::string str)
{
    for (auto iter = clientStates.begin(); iter != clientStates.end(); ++iter)
    {
        std::cout << "connfd = " << iter->first << std::endl;
        SendStr(iter->first, str);
    }
}

void MsgEnter(int connfd,std::string message)
{
    //std::cout << message << std::endl;
    std::vector<std::string> splitMsg;
    split(message, splitMsg, ',');
    clientStates[connfd]->socketStr = splitMsg[0];
    clientStates[connfd]->posX = std::stof(splitMsg[1]);
    clientStates[connfd]->posY = std::stof(splitMsg[2]);
    clientStates[connfd]->dirX = std::stoi(splitMsg[3]);
    clientStates[connfd]->dirY = std::stoi(splitMsg[4]);

    //std::cout << clientStates[connfd]->posX << std::endl;
    std::string sendStr = "Enter|" + message;

    BroadCast(sendStr);
}

void MsgList(int connfd)
{
    std::stringstream ss;
    ss << "List|";
    for (auto iter = clientStates.begin(); iter != clientStates.end(); ++iter)
    {
        ss << iter->second->socketStr << ","
           << iter->second->posX << ","
           << iter->second->posY << ","
           << iter->second->dirX << ","
           << iter->second->dirY << ",";
    }
    std::string sendStr(ss.str());
    std::cout << "List to send " << sendStr << std::endl;
    SendStr(connfd, sendStr);
}

void MsgMove(int connfd,std::string message){
    //std::cout << message << std::endl;
    std::vector<std::string> splitMsg;
    split(message, splitMsg, ',');
    clientStates[connfd]->socketStr = splitMsg[0];
    clientStates[connfd]->posX = std::stof(splitMsg[1]);
    clientStates[connfd]->posY = std::stof(splitMsg[2]);
    clientStates[connfd]->dirX = std::stoi(splitMsg[3]);
    clientStates[connfd]->dirY = std::stoi(splitMsg[4]);

    //std::cout << clientStates[connfd]->posX << std::endl;
    std::string sendStr = "Move|" + message;

    BroadCast(sendStr);
}

void MsgFire(std::string message){
    std::string sendStr = "Fire|" + message;

    BroadCast(sendStr);
}

void MsgHit(std::string message){
    std::string sendStr = "Hit|" + message;

    BroadCast(sendStr);
}

void MsgTip(std::string message){
    std::string sendStr = "Tip|" + message;

    BroadCast(sendStr);
}

void MsgText(std::string message){
    std::string sendStr = "Text|" + message;

    BroadCast(sendStr);
}

void MsgLeave(std::string message){
    std::string sendStr = "Leave|" + message;

    BroadCast(sendStr);
}

void ReceiveData(int connfd)
{
    if (clientStates[connfd]->recvbuf->Length() <= 2)
    {
        return;
    }
    int bodyLength = clientStates[connfd]->recvbuf->BodyLength();
    if (clientStates[connfd]->recvbuf->Length() < 2 + bodyLength)
    {
        return;
    }
    printf("++++++++++++++++++++++ receive date handle\n");
    clientStates[connfd]->recvbuf->readIdx += 2;

    // parse recv buffer
    char recvbuf[bodyLength + 1];
    memcpy(recvbuf, clientStates[connfd]->recvbuf->ReadAddr(), bodyLength);
    recvbuf[bodyLength] = '\0';
    std::string recvStr(recvbuf);
    std::cout << "recvStr " << recvStr << std::endl;

    clientStates[connfd]->recvbuf->readIdx += bodyLength;
    clientStates[connfd]->recvbuf->CheckAndMoveBytes();

    std::vector<std::string> messages;
    split(recvStr, messages, '|');

    std::cout << "messages[0] " << messages[0] << std::endl;
    if(strcmp(messages[0].c_str(),"Enter") == 0){
        std::cout << "MsgEnter" << std::endl;
        MsgEnter(connfd,messages[1]);
    }else if(strcmp(messages[0].c_str(),"List") == 0){
        std::cout << "MsgList" << std::endl;
        MsgList(connfd);
    }else if(strcmp(messages[0].c_str(),"Move") == 0){
        std::cout << "MsgMove" << std::endl;
        MsgMove(connfd,messages[1]);
    }else if(strcmp(messages[0].c_str(),"Fire") == 0){
        std::cout << "MsgFire" << std::endl;
        MsgFire(messages[1]);
    }else if(strcmp(messages[0].c_str(),"Hit") == 0){
        std::cout << "MsgHit" << std::endl;
        MsgHit(messages[1]);
    }else if(strcmp(messages[0].c_str(),"Tip") == 0){
        std::cout << "MsgTip" << std::endl;
        MsgTip(messages[1]);
    }else if(strcmp(messages[0].c_str(),"Text") == 0){
        std::cout << "MsgText" << std::endl;
        MsgText(messages[1]);
    }else if(strcmp(messages[0].c_str(),"Leave") == 0){
        std::cout << "MsgLeave" << std::endl;
        MsgLeave(messages[1]);
    }
    ReceiveData(connfd);
}

void activate_nonblock(int fd)
{
    int ret;
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        ERROR_EXIT("fcntl");

    flags |= O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1)
        ERROR_EXIT("fcntl");
}

int main()
{
    signal(SIGPIPE, handle_sigpipe);

    int listenfd;
    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        ERROR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1234); // server port
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // reuse addr
    int on = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        ERROR_EXIT("setsockopt");

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        ERROR_EXIT("bind");

    if (listen(listenfd, SOMAXCONN) < 0)
        ERROR_EXIT("listen");

    printf("server running...\n");
    int epollfd = epoll_create1(EPOLL_CLOEXEC);

    struct epoll_event event;
    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);

    EventList events(16);
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);

    for (;;)
    {
        int nready = epoll_wait(epollfd, &*events.begin(), static_cast<int>(events.size()), 0);
        if (nready == -1)
        {
            if (errno == EINTR)
                continue;
            ERROR_EXIT("epoll_wait");
        }
        if (nready == 0)
            continue;

        if ((size_t)nready == events.size())
            events.resize(events.size() * 2);

        for (int i = 0; i < nready; ++i)
        {
            int conn;
            if (events[i].data.fd == listenfd)
            {
                conn = accept(listenfd, (struct sockaddr *)&peeraddr, &peerlen);
                if (conn == -1)
                    ERROR_EXIT("accept");

                printf("accept fd = %d\n", conn);
                printf("ip = %s port = %d ", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
                clientStates[conn] = new ClientState();
                printf("count = %ld\n", clientStates.size());

                activate_nonblock(conn);

                event.data.fd = conn;
                event.events = EPOLLIN | EPOLLET;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, conn, &event);
            }
            else if (events[i].events & EPOLLIN)
            {
                printf("receive ");
                conn = events[i].data.fd;
                if (conn < 0)
                    continue;

                int ret = recv(conn, clientStates[conn]->recvbuf->WriteAddr(), clientStates[conn]->recvbuf->Remain(), 0);
                if (ret == -1 && errno == EINTR)
                    continue;

                if (ret < 0)
                    continue;
                else if (ret == 0)
                {
                    std::cout << "connection closed" << std::endl;
                    close(conn);
                    event = events[i];
                    std::string leavePeer = clientStates[conn]->socketStr;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn, &event);
                    delete clientStates[conn];
                    clientStates.erase(conn);
                    MsgLeave(leavePeer);
                    continue;
                }

                clientStates[conn]->recvbuf->writeIdx += ret;

                printf("message %d\n", conn);
                ReceiveData(conn);
                if (clientStates[conn]->recvbuf->Remain() < 8)
                {
                    printf("-------------------------remain = %d trigger MoveBytes\n", clientStates[conn]->recvbuf->Remain());
                    clientStates[conn]->recvbuf->MoveBytes();
                }
            }
        }
    }
    return 0;
}
