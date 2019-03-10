#pragma  once 

#include <string>
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <unordered_set>
#include <sys/time.h>

namespace common{

// 这个类包含了所有和字符串相关的操作
class StringUtil{
  public:
     // 例如, 如果待切分字符串:
     // aaa\3\3bbb
     // 如果是 compress_off, 就是切分成三个部分
     // 如果是 compress_on, 就是切分成两个部分
     // (相邻的 \3 被压缩成一个)
    static void Split(const std::string& input,std::vector<std::string>* output,
                      const std::string& spilt_char){
      boost::split(*output,input,boost::is_any_of(spilt_char),boost::token_compress_off);
    }
};

class DictUtil
{
  public:
    bool Load(const std::string& path)
    {
      std::ifstream file(path.c_str());
      if(!file.is_open())
      {
        return false;
      }
      std::string line;
      while(std::getline(file,line))
      {
        set_.insert(line);
      }
      file.close();
      return true;
    }
    bool Find(const std::string& key)const
    {
      return set_.find(key) != set_.end();
    }
  private:
    std::unordered_set<std::string> set_;  //用来存储暂停词
};

//文件读写操作工具
class FileUtil
{
  public:
    static bool Read(const std::string& input_path,std::string* content)
    {
      std::ifstream file(input_path.c_str());
      if(!file.is_open())
      {
        return false;
      }
      //如何获取到文件长度
      
      //移动文件光标位置到末尾
      file.seekg(0,file.end);
      
      //使用此方式，文件最大不能超过2G int_max == 2G
      int length = file.tellg();

      file.seekg(0,file.beg);
      content->resize(length);
      file.read(const_cast<char*>(content->data()),length);   //.data() == c_str()

      file.close();
      return true;
    }
    static bool Write(const std::string& output_path,const std::string& content)
    {
      std::ofstream file(output_path.c_str());
      if(!file.is_open())
      {
        return false;
      }
      file.write(content.data(),content.size());
      file.close();
      return true;
    }
};

//时间戳换算工具
class TimeUtil
{
  public:
    //获取到秒级时间戳
    static int64_t TimeStamp()
    {
      struct ::timeval tv;
      ::gettimeofday(&tv,nullptr);
      return tv.tv_sec;
    }
    //获取到毫秒级时间戳
    static int64_t TimeStampMs()
    {
      struct ::timeval tv;
      ::gettimeofday(&tv,nullptr);
      return tv.tv_sec * 1e3 + tv.tv_usec / 1000;
    }
    //获取到微秒级时间戳
    static int64_t TimeStampUs()
    {
      struct ::timeval tv;
      ::gettimeofday(&tv,nullptr);
      return tv.tv_sec * 1e6 + tv.tv_usec;
    }
};

} //end common
