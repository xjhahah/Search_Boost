#pragma  once 

#include <iostream>
#include <string>
#include <stdlib.h>
#include "ThreadPool.hpp"
#include "ProtocolUtil.hpp"
#include "Log.hpp"

class HttpServer{
    private:
        const int LISTEN_NUM;
        const int THREAD_NUM;
        int listen_sock;
        std::string ip;
        int port;
        ThreadPool *tpool;

    public:
        HttpServer(const std::string &ip_, const int &port_):LISTEN_NUM(5), THREAD_NUM(5)
        {
            listen_sock = -1;
            ip = ip_;
            port = port_;
        }
        void HttpInit()
        {
            listen_sock = socket(AF_INET, SOCK_STREAM, 0);
            if(listen_sock < 0){
                std::cerr << "create socket error" << std::endl;
                exit(2);
            }
            int opt = 1;
            setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in local;
            local.sin_family = AF_INET;
            local.sin_port = htons(port);
            local.sin_addr.s_addr = inet_addr(ip.c_str());

            if(bind(listen_sock, (struct sockaddr*)&local, sizeof(local)) < 0){
                std::cerr << "bind socket error" << std::endl;
                exit(3);
            }

            if(listen(listen_sock, LISTEN_NUM) < 0){
                std::cerr << "" << std::endl;
                exit(4);
            }
            tpool = new ThreadPool(THREAD_NUM);
        }

        void Start()
        {
            struct sockaddr_in peer;
            Task t;
            for( ; ; ){
                socklen_t len = sizeof(peer);
                int sock = accept(listen_sock, (struct sockaddr*)&peer, &len);
                if(sock < 0){
                    std::cout << "accept error" << std::endl;
                    continue;
                }
                LOG(INFO, "get a new request, add task to task queue");
                t.InitTask(Entry::HandlerRequest, sock);
                tpool->AddTask(t);
            }
        }

        ~HttpServer()
        {
            close(listen_sock);
            delete tpool;
            tpool = NULL;
        }
};
