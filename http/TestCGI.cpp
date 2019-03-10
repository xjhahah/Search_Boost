#include <iostream>
#include <stdlib.h>
#include <string>
#include <unistd.h>

int main()
{
    if(getenv("Content_Length")){
        std::string len = getenv("Content_Length");
        int len_ = atoi(len.c_str());
        std::string data;
        char c;
        while(len_){
            read(0, &c, 1);
            data.push_back(c);
            len_--;
        }

        std::cout << data << std::endl;
    }

    return 0;
}
