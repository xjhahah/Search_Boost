#pragma  once

#include <iostream>
#include <string>
#include <sys/time.h>

class LogTime {
public:
  static int64_t TimeStamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
  }
};

const char *level[] = {
    "INFO",
    "WARNING",
    "ERROR",
    "DEBUG",
};

enum log_level{
    INFO=0,
    WARNING,
    ERROR,
    DEBUG,
};

inline std::ostream& Log(log_level level_, const char* file, int line, std::string msg)
{
    std::ostream &obj = std::cout;
    std::string err_msg = level[level_];
    obj << " [ " << err_msg << ":" << LogTime::TimeStamp() << ":" << file << ":" << line << " ] " << msg << std::endl;
    return obj;
}

#define LOG(flag, msg) Log(flag, __FILE__, __LINE__, msg)

