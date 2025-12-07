#pragma once

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <atomic>
#include <fstream>
#include <boost/algorithm/string.hpp>

namespace ns_util
{
    class TimeUtil
    {
    public:
        static std::string GetTimeStamp()
        {
            struct timeval time;
            gettimeofday(&time, nullptr);
            return std::to_string(time.tv_sec);
        }

        // 获取毫秒时间戳
        static std::string GetTimeMS()
        {
            struct timeval time;
            gettimeofday(&time, nullptr);
            return std::to_string(time.tv_sec * 1000 + time.tv_usec / 1000);
        }
    };

    const std::string temp_path = "./temp/";
    class PathUtil
    {
    public:
        static std::string AddSuffix(const std::string &file_name, const std::string &suffix)
        {
            std::string path_name = temp_path;
            path_name += file_name;
            path_name += suffix;
            return path_name;
        }

        // 编译时需要的临时文件
        // 构建源文件完整路径+后缀名
        // test -> ./temp/test.cpp
        static std::string Src(const std::string &file_name)
        {
            return AddSuffix(file_name, ".cpp");
        }
        // 构建可执行文件完整路径+后缀名
        static std::string Exe(const std::string &file_name)
        {
            return AddSuffix(file_name, ".exe");
        }
        static std::string CompileError(const std::string &file_name)
        {
            return AddSuffix(file_name, ".compile_error");
        }

        // 运行时需要的临时文件
        // 构建标准错误完整的路径+后缀名
        static std::string Stderr(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stderr");
        }
        static std::string Stdin(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stdin");
        }
        static std::string Stdout(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stdout");
        }
    };

    class FileUtil
    {
    public:
        static bool IsFileExist(const std::string &file_name)
        {
            struct stat st;
            if (stat(file_name.c_str(), &st) == 0)
                return true;
            else
                return false;
        }

        // 毫秒级时间戳 + 原子性递增唯一值：来保证唯一性
        static std::string UniqFileName()
        {
            std::atomic_uint id(0);
            id++;
            std::string time = TimeUtil::GetTimeMS();
            std::string uniq_id = std::to_string(id);
            return time + "_" + uniq_id;
        }

        static bool WriteFile(const std::string &target, const std::string &content)
        {
            std::ofstream out(target);
            if (!out.is_open())
            {
                return false;
            }
            out.write(content.c_str(), content.size());
            out.close();
            return true;
        }

        static bool ReadFile(const std::string &target, std::string *content, bool keep = false)
        {
            (*content).clear();
            std::ifstream in(target);
            if (!in.is_open())
            {
                return false;
            }
            std::string line;
            while (std::getline(in, line))
            {
                (*content) += line;
                (*content) += (keep ? "\n" : "");
            }
            in.close();
            return true;
        }
    };

    class StringUtil
    {
    public:
        static void SplitString(const std::string &str, std::vector<std::string> *tokens,const std::string &sep)
        {
            boost::split(*tokens,str,boost::is_any_of(sep),boost::algorithm::token_compress_on);
        }
    };

}