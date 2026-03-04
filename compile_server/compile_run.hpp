#pragma once

#include "compiler.hpp"
#include "runner.hpp"
#include "../comm/log.hpp"
#include "../comm/util.hpp"

#include <signal.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>

namespace ns_compile_and_run
{
    using namespace ns_log;
    using namespace ns_util;
    using namespace ns_compiler;
    using namespace ns_runner;

    class CompileAndRun
    {
    public:
        // code > 0 : 进程收到了信号导致异常崩溃
        // code = 0 : 整个过程全部完成
        // code < 0 : 整个过程非运行报错（代码为空、编译报错等）
        static std::string CodeToDesc(int code, const std::string &file_name)
        {
            std::string desc;
            switch (code)
            {
            case 0:
                desc = "运行完毕";
                break;
            case -1:
                desc = "提交的代码为空";
                break;
            case -2:
                desc = "未知错误";
                break;
            case -3:
                // desc = "代码编译时发生错误";
                FileUtil::ReadFile(PathUtil::CompileError(file_name),&desc,true);
                break;
            case SIGABRT: // 6
            case SIGSEGV: // 11
                desc = "运行错误：内存越界或段错误";
                break;
            case SIGXCPU: // 24
                desc = "运行超时";
                break;
            case SIGFPE: // 8
                desc = "运行错误：浮点数溢出或除零";
                break;
            default:
                desc = "运行错误：收到信号 " + std::to_string(code);
                break;
            }
            return desc;
        }

        static void RemoveTempFile(const std::string& file_name)
        {
            std::string file_src = PathUtil::Src(file_name);
            if(FileUtil::IsFileExist(file_src)) unlink(file_src.c_str());

            std::string file_compiler_error = PathUtil::CompileError(file_name);
            if(FileUtil::IsFileExist(file_compiler_error)) unlink(file_compiler_error.c_str());

            std::string file_execute = PathUtil::Exe(file_name);
            if(FileUtil::IsFileExist(file_execute)) unlink(file_execute.c_str());

            std::string file_stdin = PathUtil::Stdin(file_name);
            if(FileUtil::IsFileExist(file_stdin)) unlink(file_stdin.c_str());

            std::string file_stdout = PathUtil::Stdout(file_name);
            if(FileUtil::IsFileExist(file_stdout)) unlink(file_stdout.c_str());

            std::string file_stderr = PathUtil::Stderr(file_name);
            if(FileUtil::IsFileExist(file_stderr)) unlink(file_stderr.c_str());
        }

        /*************************************
         * 输入：
         * code：用户提交的代码
         * input：用户给自己提交的代码对应的输入，不做处理
         * cpu_limit：时间要求
         * mem_limit：空间要求
         *
         * 输出：
         * 必填
         * status：状态码
         * reason：请求结果
         * 选填
         * stdout：我的程序运行完的结果
         * stderr：我的程序运行完错误的结果
         * in_json:{"code":"#include...","input":"","cpu_limit":1,"mem_limit":10240}
         * out_json:{"status":"0","reason":"","stdout":"","stderr":""}
         *************************************/
        static void Start(std::string &in_json, std::string *out_json)
        {
            Json::Value in_value;
            Json::Reader reader;
            reader.parse(in_json, in_value); // 最后再处理差错问题

            std::string code = in_value["code"].asString();
            std::string input = in_value["input"].asString();
            int cpu_limit = in_value["cpu_limit"].asInt();
            int mem_limit = in_value["mem_limit"].asInt();

            Json::Value out_value;
            int status_code = 0;
            int run_result = 0;
            std::string file_name; // 需要内部形成的唯一文件名

            if (code.size() == 0)
            {
                // 差错处理
                status_code = -1;
                goto END;
            }

            // 形成的文件名只具有唯一性，没有目录没有后缀
            file_name = FileUtil::UniqFileName();

            // 形成临时src文件
            if (!FileUtil::WriteFile(PathUtil::Src(file_name), code))
            {
                status_code = -2; // 未知错误
                goto END;
            }

            if (!Compiler::Compile(file_name))
            {
                // 编译失败
                status_code = -3; // 代码编译的时候发生了错误
                goto END;
            }

            run_result = Runner::Run(file_name, cpu_limit, mem_limit);
            if (run_result < 0)
            {
                status_code = -2; // 未知错误
                goto END;
            }
            else if (run_result > 0)
            {
                status_code = run_result;
                goto END;
            }
            else
            {
                // 运行成功
                status_code = 0;
            }
        END:
            out_value["status"] = status_code;
            out_value["reason"] = CodeToDesc(status_code, file_name);
            if (status_code == 0)
            {
                // 整个过程全部成功
                std::string value_stdout;
                FileUtil::ReadFile(PathUtil::Stdout(file_name), &value_stdout, true);
                out_value["stdout"] = value_stdout;
                std::string value_stderr;
                FileUtil::ReadFile(PathUtil::Stderr(file_name), &value_stderr, true);
                out_value["stderr"] = value_stderr;
            }

            Json::StyledWriter writer;
            *out_json = writer.write(out_value);

            //调试时关闭
            RemoveTempFile(file_name);
        }
    };
}
