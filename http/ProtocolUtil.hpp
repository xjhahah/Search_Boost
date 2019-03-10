#pragma  once

#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Log.hpp"

#define HTTP_VERSION "HTTP/1.0 "
#define HTTP_ROOT "wwwroot"
#define PAGE_404 "wwwroot/404.html" 
#define HOME_PAGE "index.html"

typedef enum{
    OK=200,
    NOT_FOUND=404,
}status_t;

typedef std::unordered_map<std::string, std::string> Param_t;

Param_t g_text_type{
    {".html", "text/html\n"},
    {".js", "application/x-javascript\n"},
    {".css", "text/css\n"},
};

class StringUtil{
    public:
        static bool MakeKV(Param_t &param_map, std::string &s)
        {
            std::string sep_ = ": ";
            std::size_t pos = s.find(sep_);
            if(std::string::npos == pos){
                return false;
            }

            std::string k_ = s.substr(0, pos);
            std::string v_ = s.substr(pos+sep_.size());
            param_map.insert(std::make_pair(k_, v_));
            return true;
        }
};

class ProtocolUtil{
    public:
        static std::string CodeToStringDesc(status_t &code)
        {
            switch(code){
                case 200:
                    return " OK\n";
                case 404:
                    return " NOT_FOUND\n";
                default:
                    return " UNKNOW\n";
            }
        }
};

class Request{
    public:
        std::string rq_line;
        std::string rq_header;
        std::string blank_line;
        std::string rq_text;
        bool is_done;
    public:
        std::string method;
        std::string url;
        std::string http_version;
        Param_t header_kv;
        std::string path;
        std::string rq_param;
        std::string resource_suffix;
        int content_length;
        int resource_size;
        bool cgi;
    public:
        Request():cgi(false),resource_size(-1),content_length(-1),blank_line("\n"), is_done(false)
        {}
        //提取method，url，http version
        void RequestLineParse()
        {
            std::stringstream ss(rq_line);
            ss >> method >> url >> http_version;
        }
        //检测方法是否合法，设置cgi
        bool IsMethodVaild()
        {
            if((cgi = (strcasecmp(method.c_str(), "POST") == 0)) || strcasecmp(method.c_str(), "GET") == 0){
                return true;
            }
            return false;
        }
        //分离url中资源路径，是否有参,设置CGI
        void RequestUrlParse()
        {
            int pos = url.find("?");
            if(std::string::npos != pos){
                rq_param = url.substr(pos+1);
                url = url.substr(0, pos);
                cgi = true;
            }
            path = HTTP_ROOT;
            path += url;
            if(path[path.size() - 1] == '/'){
                path += HOME_PAGE;
            }
        }
        //检测资源路径合法性，设置CGI，填充资源后缀
        bool IsPathVaild()
        {
            struct stat st;
            if(stat(path.c_str(), &st)){
                return false;
            }else{
                if(S_ISDIR(st.st_mode)){
                    path += "/";
                    path += HOME_PAGE;
                }else if((st.st_mode & S_IXUSR) ||\
                        (st.st_mode & S_IXGRP) ||\
                        (st.st_mode & S_IXOTH)){
                    cgi = true;
                }else{
                    //DO NOTHING
                }
            }
            resource_size = st.st_size;
            std::size_t pos_ = path.find_last_of('.');
            if(pos_ != std::string::npos){
                resource_suffix = path.substr(pos_); //.css .html .js
            }else{
                resource_suffix = ".html"; //默认.html
            }
            return true;
        }
        //逐行解析，转化kv，存入unordered_map. POST方法，提取content_length
        void HeaderParse()
        {
            std::size_t pos = 0;
            std::size_t start = 0;
            while( start < rq_header.size() && std::string::npos != (pos = rq_header.find('\n', start)) ){
                std::string sub_ = rq_header.substr(start, pos - start);
                StringUtil::MakeKV(header_kv, sub_);
                start = pos + 1;
            }
            if(strcasecmp(method.c_str(), "POST") == 0){
                std::string cl_ = header_kv["Content-Length"];
                if(cl_.empty()){
                    content_length = -1;
                }else{
                    std::stringstream ss;
                    ss << cl_;
                    ss >> content_length;
                }
            }
        }
        void Make404Request()
        {
            path = PAGE_404;
            resource_suffix = ".html";
            struct stat st;
            stat(path.c_str(), &st);
            resource_size = st.st_size;
        }
        ~Request()
        {}
};

class Response{
    public:
        std::string rsp_line;
        std::string rsp_header;
        std::string blank_line;
        std::string rsp_text;
    public:
        status_t code;
        int content_length;
        std::string resource_suffix;
        int fd;
    //    bool cgi;

    public:
        Response():code(OK),blank_line("\n"), fd(-1)//,cgi(false)
        {}
        void BuildResponseHeader()
        {
            std::stringstream ss;
            ss << code;

            rsp_line  = HTTP_VERSION;
            rsp_line += ss.str();
            rsp_line += ProtocolUtil::CodeToStringDesc(code);

            rsp_header  = "Content-Type: ";
            rsp_header += g_text_type[resource_suffix];

            rsp_header += "Content-Length: ";
            ss.str("");
            ss << content_length<< "\n";
            rsp_header += ss.str();
            
            rsp_header += blank_line;
        }
        ~Response()
        {}
};

class Connect{
    private:
        int sock;
    public:
        Connect(int sock_):sock(sock_)
        {}
        //读取请求行或者任意请求行
        int ReadOneLine(std::string &line_)
        {
            char c = 'x';
            while(c != '\n'){
                ssize_t s = recv(sock, &c, 1, 0);
                if(s > 0){
                    if(c == '\r'){
                        recv(sock, &c, 1, MSG_PEEK);
                        if(c == '\n'){
                            recv(sock, &c, 1, 0);
                        }
                        else{
                            c = '\n';
                        }
                    }
                    line_.push_back(c);
                }
                else{
                    break;
                }
            }
            return line_.size();
        }
        //读取请求包头,包括空行
        void ReadHeader(std::string &header_)
        {
            std::string line_;
            while(line_ != "\n"){
                line_ = "";
                ReadOneLine(line_);
                header_ += line_;
            }
        }
        //读取请求正文
        bool ReadText(std::string &text_, int content_length_)
        {
                char c_;
                ssize_t s_;
                while( content_length_ > 0 ){
                    s_ = recv(sock, &c_, 1, 0);
                    if(s_ <= 0){
                        break;
                    }
                    content_length_--;
                    text_.push_back(c_);
                }
                if(content_length_ != 0){
                    return false;
                }
                return true;
        }
        void SendResponseHeader(Response *&rsp_)
        {
            std::string &rsp_line_ = rsp_->rsp_line;
            std::string &rsp_header_ = rsp_->rsp_header;

            send(sock, rsp_line_.c_str(), rsp_line_.size(), 0);
            send(sock, rsp_header_.c_str(), rsp_header_.size(), 0);
        }
        void SendResponseText(Response *&rsp_, bool is_cgi_)
        {
            std::string &text_ = rsp_->rsp_text;

            if(is_cgi_){
                send(sock, text_.c_str(),text_.size(), 0);
            }else{
                sendfile(sock, rsp_->fd, NULL, rsp_->content_length);
            }
        }

        ~Connect()
        {
            close(sock);
        }
};

class Entry{
    public:
        //处理非CGI请求
        static void ProcessNonCgi(Connect *&conn_, Request *&rq_, Response *&rsp_)
        {
            std::string &path_ = rq_->path;
            int &fd_ = rsp_->fd;
            fd_ =  open(path_.c_str(), O_RDONLY);
            if(fd_ < 0){
                rsp_->code = NOT_FOUND;
                return;
            }
            
            rsp_->content_length = rq_->resource_size;
            rsp_->resource_suffix  = rq_->resource_suffix;

            rsp_->BuildResponseHeader();
            conn_->SendResponseHeader(rsp_);
            conn_->SendResponseText(rsp_, false);
            close(fd_);
        }
        static void ProcessError(Connect *&conn_, Request *&rq_, Response *&rsp_)
        {
            switch(rsp_->code){
                case 404:
                    rq_->Make404Request();
                    break;
                case 501:
                    break;
                case 503:
                    break;
                default:
                    break;
            }
            ProcessNonCgi(conn_, rq_, rsp_);
        }
        static void ProcessCgi(Connect *&conn_, Request *&rq_, Response *&rsp_)
        {
            std::string &param = rq_->rq_param;
            int param_size_ = param.size();

            std::stringstream ss;
            ss << param_size_;
            std::string str_param_size_ = ss.str();

            //LOG(DEBUG, param);
            int input[2];
            int output[2];
            pipe(input);
            pipe(output);

            pid_t id = fork();
            if(id < 0){
                rsp_->code = NOT_FOUND;
                return;
            }else if(id == 0){ //child
                //LOG(DEBUG, path_);
                close(input[1]); // child read
                close(output[0]); //child write
                dup2(input[0], 0);
                dup2(output[1], 1);


                std::string content_length_env_ = "Content_Length=";
                content_length_env_ += str_param_size_;
                putenv((char*)content_length_env_.c_str());

                std::string &path_ = rq_->path;

                execl(path_.c_str(), path_.c_str(), NULL);
                exit(1);
            }
            else{//father
                close(input[0]);
                close(output[1]);

                int total = 0;
                ssize_t s = 0;
                const char *str = param.c_str();

                while( total < param_size_ && (s = write(input[1], str + total, param_size_ - total)) > 0 ){
                    total += s;
                }

                int &len_ = rsp_->content_length;
                std::string &text_ = rsp_->rsp_text;

                len_ = 0;
                char c;
                while(read(output[0], &c, 1) > 0){
                    text_.push_back(c);
                    len_++;
                }

                rsp_->resource_suffix = ".html";

                rsp_->BuildResponseHeader();
                conn_->SendResponseHeader(rsp_);
                conn_->SendResponseText(rsp_, true);

                waitpid(id, NULL, 0);
                close(input[1]);
                close(output[0]);
            }
        }
        //处理http，读取所有请求
        static void HttpProcess(Connect *&conn_, Request *&rq_, Response *&rsp_)
        {
            LOG(INFO, "request process begin!");
            if(rq_->cgi){
                LOG(INFO, "request handler througt cgi");
                ProcessCgi(conn_, rq_, rsp_);
            }else{
                LOG(INFO, "request handler througt non_cgi");
                ProcessNonCgi(conn_, rq_, rsp_);
            }
            LOG(INFO, "request process end!");
        }

        static void HandlerRequest(int sock_)
        {
            Connect *conn_ = new Connect(sock_);
            Request *rq_   = new Request();
            Response *rsp_ = new Response();

            conn_->ReadOneLine(rq_->rq_line);
            rq_->RequestLineParse();

            if(!rq_->IsMethodVaild()){
                LOG(ERROR, "request method error!");
                rsp_->code = NOT_FOUND;
                conn_->ReadHeader(rq_->rq_header);
                goto end;
            }
            rq_->RequestUrlParse();

            if(!rq_->IsPathVaild()){
                LOG(ERROR, "request path error!");
                conn_->ReadHeader(rq_->rq_header);
                rsp_->code = NOT_FOUND;
                goto end;
            }
            conn_->ReadHeader(rq_->rq_header);
            rq_->HeaderParse();
            if( strcasecmp((rq_->method).c_str(), "POST") == 0 && rq_->content_length > 0){
                if( !conn_->ReadText(rq_->rq_text, rq_->content_length) ){
                    rsp_->code = NOT_FOUND;
                    goto end;
                }
            }
            HttpProcess(conn_, rq_, rsp_);
        end:
            if(rsp_->code != OK){
                ProcessError(conn_, rq_, rsp_);
            }
            delete conn_;
            delete rq_;
            delete rsp_;
        }

};
