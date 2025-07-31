#pragma once

#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../comm/util.hpp"

//只负责对代码的编译

namespace ns_compiler
{
    using namespace ns_util;
    class Compiler
    {
    public:
        Compiler()
        {}
        ~Compiler()
        {}

        //filename -> test
        //test -> ./temp/test.cpp
        //test -> ./temp/test.exe
        //test -> ./temp/test.stderr
        static bool Compile(const std::string &file_name)
        {
            pid_t pid = fork();
            if(pid<0)
            {
                return false;
            }
            else if(pid == 0)
            {
                int _stderr = open(PathUtil::Stderr(file_name).c_str(),O_CREAT | O_WRONLY ,0644);
                if(_stderr < 0)
                {
                    exit(1);
                }
                dup2(_stderr,2);//将原本显示到2号文件描述符的内容，显示到_stderr文件中
                
                //程序替换，并不影响进程的文件描述符表
                //子进程：调用编译器，完成对代码的编译工作
                //g++ -o test.exe test.cpp -std=c++11
                execlp("g++","-o",PathUtil::Exe(file_name).c_str(),PathUtil::Src(file_name).c_str(),"-std=c++11",nullptr);
                exit(2);
            }
            else
            {
                //father
                waitpid(pid,nullptr,0);
                //判断是否创建成功
                if(FileUtil::IsFileExist(PathUtil::Exe(file_name))) 
                    return true;
            }
            return false;
        }
    };
}
