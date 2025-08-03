#pragma once

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

namespace ns_util
{
    const std::string temp_path = "./temp/";
    class PathUtil
    {
    public:
        static std::string AddSuffix(const std::string &file_name,const std::string &suffix)
        {
            std::string path_name = temp_path;
            path_name += file_name;
            path_name += suffix;
            return path_name;
        }

        //构建源文件完整路径+后缀名
        //test -> ./temp/test.cpp
        static std::string Src(const std::string &file_name)
        {
            return AddSuffix(file_name,".cpp");
        }
        //构建可执行文件完整路径+后缀名
        static std::string Exe(const std::string &file_name)
        {
            return AddSuffix(file_name,".exe"); 
        }
        //构建标准错误完整的路径+后缀名
        static std::string Stderr(const std::string &file_name)
        {
            return AddSuffix(file_name,".stderr");
        }
    };

    class FileUtil
    {
    public:
        static bool IsFileExist(const std::string& file_name)
        {
            struct  stat st;
            if(stat(file_name.c_str(),&st) == 0) 
                return true;
            else 
                return false;
            
        }
    };

    class TimeUtil
    {
    public:
        static std::string GetTimeStamp()
        {
            struct timeval tv;
            gettimeofday(&tv,nullptr);
            return std::to_string(tv.tv_sec);
        }
    };
}