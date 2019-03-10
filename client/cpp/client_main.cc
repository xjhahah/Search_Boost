//让搜索客户端能和HTTP服务器关联到一起，
//就需要让客户端能够支持 CGI 协议
//CGI约定了我们的动态页面该怎么样显示

#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>

#include "server.pb.h"

#include "../../common/util.hpp"


DEFINE_string(server_addr,"127.0.0.1:8080", "服务器端的地址");

namespace doc_client{

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;

int GetQueryString(char output[]) {
  // 1. 先从环境变量中获取到方法
  char* method = getenv("REQUEST_METHOD");
  if (method == NULL) {
    fprintf(stderr, "REQUEST_METHOD failed\n");
    return -1;
  }
  // 2. 如果是 GET 方法, 就是直��从环境变量中
  //    获取到 QUERY_STRING
  if (strcasecmp(method, "GET") == 0) {
    char* query_string = getenv("QUERY_STRING");
    if (query_string == NULL) {
      fprintf(stderr, "QUERY_STRING failed\n");
      return -1;
    }
    strcpy(output, query_string);
  } else {
    // 3. 如果是 POST 方法, 先通过环境变量获取到 CONTENT_LENGTH
    //    再从标准输入中读取 body
    char* content_length_str = getenv("CONTENT_LENGTH");
    if (content_length_str == NULL) {
      fprintf(stderr, "CONTENT_LENGTH failed\n");
      return -1;
    }
    int content_length = atoi(content_length_str);
    int i = 0;  // 表示当前已经往  output 中写了多少个字符了
    for (; i < content_length; ++i) {
      read(0, &output[i], 1);
    }
    output[content_length] = '\0';
  }
  return 0;
}

void PackageRequest(Request* req)
{
  req->set_sid(0);
  req->set_timestamp(common::TimeUtil::TimeStamp());
  
  char query_string[1024*4] = {0};
  GetQueryString(query_string);
  //获取到的query_string 的内容格式形如：
  //query = filesystem
  char query[1024*4] = {0};
  sscanf(query_string,"query=%s",query); //字符串处理函数,但是并不太好，容错率太低，字符串切分

  req->set_query(query);
  std::cerr << "query_string= " <<query_string <<std::endl;
  std::cerr << "query= " <<query <<std::endl;
}

//基于RPC方式调用服务器对应的Add函数
void Search(const Request& req,Response* resp)
{
  using namespace sofa::pbrpc;
  //1、RPC概念1 RpcClient
  RpcClient  client;
  //2、RPC概念2 RpcChannel，描述一次连接
  RpcChannel channel(&client,fLS::FLAGS_server_addr);
  //3、RPC概念3 DocServerAPI_Stub 描述这次请求具体调用那个RPC函数
  doc_server_proto::DocServerAPI_Stub stub(&channel);

  //4、RPC概念四 RpcController 网络通信细节的管理对象
  //管理网络通信中的五元组，以及超时时间等概念
  RpcController ctrl;

  //把第四个参数设为NULL，表示该请求按照同步方式进行请求
  stub.Search(&ctrl,&req,resp,NULL);

  if(ctrl.Failed())
  {
    std::cerr << "RPC Add failed..." << std::endl;
  }
  else 
  {
    std::cerr << "RPC Add Success..." << std::endl;
  }
  return;
}

/*
void ParseResponse(const Response& resp)
{
  //std::cout << resp.Utf8DebugString()  <<std::endl;
  //期望的输出结果是HTML格式的数据
  std::cout << "<html>";
  std::cout << "<body>";
  for(int i=0; i < resp.item_size(); ++i)
  {
    const auto& item = resp.item(i);
    std::cout << "<div>";
    //拼装标题和 jump_url 对应的html
    std::cout << "<div><a href=\">" << item.jump_url() << "\">" << item.title() << "</a></div>";
    
    //拼装描述
    std::cout << "<div>" << item.desc() << "</div>";
    //拼装 show_url   
    std::cout << "<div>" << item.show_url() << "</div>";
    std::cout << "</div>";
  }
  std::cout << "</body>";
  std::cout << "</html>";
}
*/

void ParseResponse(const Response& resp) {
  // std::cout << resp.Utf8DebugString();
  // 期望的输出结果是 HTML 格式的数据
  std::cout << "<html>";
  std::cout << "<body>";
  for (int i = 0; i < resp.item_size(); ++i) {
    const auto& item = resp.item(i);
    std::cout << "<div>";
    // 拼装标题和 jump_url 对应的 html
    std::cout << "<div><a href=\"" << item.jump_url()
              << "\">" << item.title() << "</a></div>";
    // 拼装描述
    std::cout << "<div>" << item.desc() << "</div>";
    // 拼装 show_url
    std::cout << "<div>" << item.show_url() << "</div>";
    std::cout << "</div>";
  }
  std::cout << "</body>";
  std::cout << "</html>";
}
//此函数负责一次请求的完整过程
void CallServer()
{
  //1、构造请求
  Request req;
  Response resp;
  PackageRequest(&req);

  //2、发送给服务器，并获得响应
  Search(req,&resp);

  //2、解析响应并打印结果
  ParseResponse(resp);

  return;
}

} //end doc_client 


int main(int argc,char* argv[])
{
  base::InitApp(argc,argv);
  doc_client::CallServer();
  return 0;
}
