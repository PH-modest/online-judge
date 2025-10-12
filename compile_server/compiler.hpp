#pragma once

#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../comm/util.hpp"
#include "../comm/log.hpp"

//只负责对代码的编译

namespace ns_compiler
{
    using namespace ns_util;
    using namespace ns_log;
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
                LOG(ERROR) << "创建子进程失败！" << "\n";
                return false;
            }
            else if(pid == 0)
            {
                umask(0);
                int _stderr = open(PathUtil::CompileError(file_name).c_str(),O_CREAT | O_WRONLY ,0644);
                if(_stderr < 0)
                {
                    LOG(WARNING)<<"stderr文件创建失败！"<<"\n";
                    exit(1);
                }
                dup2(_stderr,2);//将原本显示到2号文件描述符的内容，显示到_stderr文件中
                
                //程序替换，并不影响进程的文件描述符表
                //子进程：调用编译器，完成对代码的编译工作
                //g++ -o test.exe test.cpp -std=c++11
                execlp("g++","g++","-o",PathUtil::Exe(file_name).c_str(),PathUtil::Src(file_name).c_str(),"-std=c++11",nullptr);
                LOG(ERROR)<<"启动编译器g++失败！"<<"\n";
                exit(2);
                
            }
            else
            {
                //father
                waitpid(pid,nullptr,0);
                //判断是否创建成功
                if(FileUtil::IsFileExist(PathUtil::Exe(file_name))) 
                {
                    LOG(INFO)<<PathUtil::Src(file_name)<<" 编译成功！"<<"\n";
                    return true;
                }
            }
            LOG(ERROR)<<"编译失败，没有形成可执行文件！"<<"\n";
            return false;
        }
    };
}
