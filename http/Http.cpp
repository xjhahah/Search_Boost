#include "Http.hpp" 
#include "ProtocolUtil.hpp"
#include "ThreadPool.hpp"

static void Usage(std::string name_)
{
    std::cout << "Usage: " << name_ << " ip port" << std::endl;
}

int main(int argc, char *argv[])
{
    if(argc != 3){
        Usage(argv[0]);
        exit(1);
    }
    std::string ip = argv[1];
    int port = atoi(argv[2]);
    HttpServer *serverp = new HttpServer(ip, port);
    serverp->HttpInit();
    serverp->Start();
    delete serverp;
    return 0;
}
