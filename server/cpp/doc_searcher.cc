#include "doc_searcher.h"

#include "../../common/util.hpp"
#include "../../index/cpp/index.h"

DEFINE_int32(desc_max_size, 160 , "描述的最大长度");

namespace doc_server{

//实现核心类具体内容

bool DocSearcher::Search(const Request& req, Response* resp)
{
  Context context;
  context.req = &req;
  context.resp = resp;

  //1.对查询词进行分词
  CutQuery(&context); 
  //2.根据分词结果获取倒排拉链
  Retrieve(&context);
  //3.根据触发结果进行排序
  Rank(&context);
  //4.根据排序结果，包装响应
  PackageResponse(&context);
  //5.记录日志
  Log(&context);
  
  return true;
}

bool DocSearcher::CutQuery(Context* context)
{
  //基于 jieba 分词对查询词进行分词
  //分词结果保存在 context->words
  //当前的分词，是需要干掉暂停词的
  //暂停词表，在索引中已经加载过一次
  //修改索引，在索引中再新增一个API函数，用来完成过滤暂停词的分词
  
  doc_index::Index* index = doc_index::Index::Instance();
  index->CutWordWithoutStopWord(context->req->query(),&context->words);
  return true;
}

bool DocSearcher::Retrieve(Context* context)
{
  doc_index::Index* index = doc_index::Index::Instance();
  //根据每个分词结果，都去查倒排
  //把查到的倒排拉链，统一放到 context->all_query_chain
  //得到一个合并后的倒排拉链（包含了不同的关键词对应的拉链的集合）
  for(const auto& word : context->words)
  {
    const doc_index::InvertedList* inverted_list = index->GetInvertedList(word);
    if(inverted_list == nullptr)
    {
      //当前关键词如果没在任何文档中出现过，倒排结果就为 NULL
      continue;
    }
    //对当前获取到的倒排拉链进行合并操作
    for (const doc_index::Weight& weight : *(inverted_list)) 
    {
      context->all_query_chain.push_back(weight);                
    }
  }
  return true;
}

bool DocSearcher::Rank(Context* context)
{
  //直接按照权重进行降序排序
  //all_query_chain 包含的是若干个关键词 倒排拉链
  //混合在一起的结果，就需要进行重新排序
  std::sort(context->all_query_chain.begin(),context->all_query_chain.end(),
            doc_index::Index::CmpWeight);
  return true;
}
////////////////////////////////////////////////////////////////////// 
// 前面已经对倒排拉链已经排过序，现在的都得仍然是一个有序的大的列表
// 这里的问题就类似吧两个有序链表合并成一个有序的链表
// 只不过此处我们是吧Nge有序的 vector 合并合成一个有序的vector
//////////////////////////////////////////////////////////////////////

bool DocSearcher::PackageResponse(Context* context)
{
  //构造出Response对象
  //Response最核心的是 Item 元素
  //为了构造 Item 元素，需要先查正排索引
  
  doc_index::Index* index = doc_index::Index::Instance();
  const Request* req = context->req;
  Response* resp = context->resp;
  resp->set_sid(req->sid());
  resp->set_timestamp(common::TimeUtil::TimeStamp());
  for(const auto& weight : context->all_query_chain)
  {
    //根据weight中的doc_id，去查正排
    const doc_index::DocInfo* doc_info = index->GetDocInfo(weight.doc_id());
    auto* item = resp->add_item();
    item->set_title(doc_info->title());
    //doc_info中包含的是 标题+ 正文
    //item中需要的是 标题+描述
    //描述信息是正文中的一段摘要，这个摘要包含查询词的部分信息
    //这里要做的就是根据正文生成描述信息
    item->set_desc(GenDesc(doc_info->content(),weight.first_pos()));
    item->set_jump_url(doc_info->jump_url());
    item->set_show_url(doc_info->show_url());
  }
  return true;
}

//first_pos 中记录了当前关键词在文档正文中第一次出现的位置
//根据这个位置，把周边信息提取出来，作为描述就可以了

std::string DocSearcher::GenDesc(const std::string& content,int first_pos)
{
  int desc_beg = 0;
  //根据first_pos 去查找句子的开始位置
  //first_pos表示的含义是当前词在正文中第一次出现的位置
  //她如果当前词只是在文档标题中出现过一次而没在正文中出现过
  //对于这种情况，first_pos == -1；
  if(first_pos != -1)
  {
    desc_beg = FindSentenceBeg(content,first_pos);
  }
  std::string desc; //保存描述结果
  if(desc_beg + FLAGS_desc_max_size >= (int64_t)content.size()){
  //说明剩下的内容不足以达到我们的描述最大长度，
  //就把剩下的正文统统作为描述符
    desc = content.substr(desc_beg); //取字符串的子串
  }
  else{
    //剩下的内容超过了描述的最大长度
    desc = content.substr(desc_beg, fLI::FLAGS_desc_max_size);
    //需要把倒数三个字节设置为 .
    desc[desc.size()-1] = '.';
    desc[desc.size()-2] = '.';
    desc[desc.size()-3] = '.';
  }

  //接下来，需要对描述符的特殊字符进行转换
  //此处构造出来的 desc信息 后续会作为 html 中的一部分
  //就要求 desc中不能包含 html 中的一些特殊字符
  //转义字符的转义
  
  ReplaceEscape(&desc);
  return desc;
}

int DocSearcher::FindSentenceBeg(const std::string& content, int first_pos)
{
  //从first_pos开始从后往前遍历，找到第一个句子的分隔符就可以了
  //分隔符：，。；！？
  for(int cur = first_pos; cur >= 0;--cur)
  {
    if(content[cur] == ','|| content[cur] == '.'|| content[cur] == '!'
       ||content[cur] == '?'||content[cur] == ';')
    {
      //说明cur指向了上个句子的末尾，cur+1就是下个句子的开头
      return cur+1;
    }

    //如果循环结束了还没找到句子的分隔符，取正文的开始位置（0）
    //作为句子的开始
    return 0;
  }
}

void DocSearcher::ReplaceEscape(std::string* desc)
{
  //需要替换的转义字符有：
  //  "" -> &quot;
  //  &  -> &amp;
  //  < -> &lt;
  //  > -> &gt;
  //
  //先替换 & ，避免后续操作出现问题
  boost::algorithm::replace_all(*desc,"&", "&amp;"); 
  boost::algorithm::replace_all(*desc,"<", "&lt;"); 
  boost::algorithm::replace_all(*desc,">", "&gt;"); 
  boost::algorithm::replace_all(*desc,"\"", "&quot;");

  return;
}

bool DocSearcher::Log(Context* context)
{
  //记录下一次请求过程中涉及到的请求详细信息和响应详细信息
  LOG(INFO) << "[Request]" << context->req->Utf8DebugString();
  LOG(INFO) << "[Response]" << context->resp->Utf8DebugString();
  return true;
}

}   //end doc_server 
