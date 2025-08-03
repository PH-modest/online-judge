#pragma once
#include <iostream>
#include <string>

#include "util.hpp"

namespace ns_log
{
    using namespace ns_util;

    enum{
        INFO,
        DEBUG,
        WARNING,
        ERROR,
        FATAL
    };

    inline std::ostream &Log(const std::string& level,const std::string& file_name,int line)
    {
        // 添加日志等级
        std::string message = "[";
        message += level;
        message += "]";

        //添加报错文件名
        message+="[";
        message+=file_name;
        message+="]";

        //添加报错所在行数
        message+="[";
        message+=std::to_string(line);
        message+="]";

        //添加时间戳
        message+="[";
        message+=TimeUtil::GetTimeStamp();
        message+="]";

        std::cout<<message;//将message加入缓冲区，不要使用endl刷新缓冲区
        return std::cout;
    }

    //LOG(INFO)<<"message"<<endl;
    #define LOG(level) Log(#level,__FILE__,__LINE__)

}