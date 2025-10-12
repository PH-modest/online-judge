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

        //编译时需要的临时文件
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
        static std::string CompileError(const std::string &file_name)
        {
            return AddSuffix(file_name,".compile_error");
        }
        
        //运行时需要的临时文件
        //构建标准错误完整的路径+后缀名
        static std::string Stderr(const std::string &file_name)
        {
            return AddSuffix(file_name,".stderr");
        }
        static std::string Stdin(const std::string &file_name)
        {
            return AddSuffix(file_name,".stdin");
        }
        static std::string Stdout(const std::string &file_name)
        {
            return AddSuffix(file_name,".stdout");
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

        static std::string UniqFileName()
        {
            return "";
        }

        static bool WriteFile(const std::string& target, const std::string &code)
        {
            
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