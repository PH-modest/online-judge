#pragma once

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../comm/log.hpp"
#include "../comm/util.hpp"

namespace ns_runner
{
    using namespace ns_log;
    using namespace ns_util;

    class Runner
    {
    public:
        Runner()
        {}
        ~Runner()
        {}
    public:
        //只判断程序运行时是否异常
        //返回值 > 0 ：程序异常了，退出时收到了信号，返回值就是对应的信号编号
        //返回值 == 0：正常运行结束，结果保存在文件中
        //返回值 < 0：内部错误
        static int Run(const std::string& file_name)
        {
            std::string _execute = PathUtil::Exe(file_name);
            std::string _stdin = PathUtil::Stdin(file_name);
            std::string _stdout = PathUtil::Stdout(file_name);
            std::string _stderr = PathUtil::Stderr(file_name);

            umask(0);
            int _stdin_fd = open(_stdin.c_str(),O_CREAT | O_RDONLY,0644);
            int _stdout_fd = open(_stdout.c_str(),O_CREAT|O_WRONLY,0644);
            int _stderr_fd = open(_stderr.c_str(),O_CREAT|O_WRONLY,0644);

            if(_stdin_fd < 0 || _stderr_fd < 0 || _stdout_fd < 0)
            {
                return -1;//打开文件失败
            }

            pid_t pid = fork();
            if(pid < 0)
            {
                close(_stdin_fd);
                close(_stdout_fd);
                close(_stderr_fd);
                return -2;//代表创建子进程失败
            }
            else if(pid == 0)
            {
                dup2(_stdin_fd,0);
                dup2(_stdout_fd,1);
                dup2(_stderr_fd,2);

                execl(_execute.c_str()/*我要执行谁*/,_execute.c_str()/*我想要怎么执行*/,nullptr);
                exit(1);
            }
            else 
            {
                close(_stdin_fd);
                close(_stdout_fd);
                close(_stderr_fd);
                int status = 0;
                waitpid(pid,&status,0);
                //程序运行异常，一定是因为收到了信号
                return status & 0x7f;
            }
        }
    }
}