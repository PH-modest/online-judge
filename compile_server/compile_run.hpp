#pragma once

#include "compiler.hpp"
#include "runner.hpp"
#include "../comm/log.hpp"
#include "../comm/util.hpp"

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
     * 
     *************************************/
        static void Start(std::string& in_json, std::string *out_json)
        {
            Json::Value in_value;
            Json::Reader reader;
            reader.parse(in_json,in_value);

            std::string code = in_value["code"].asString();
            std::string input = in_value["input"].asString();
            int cpu_limit = in_value["cpu_limit"].asInt();
            int mem_limit = in_value["mem_limit"].asInt();

            if(code.size() == 0)
            {

            }

            // 形成的文件名只具有唯一性，没有目录没有后缀
            std::string file_name = FileUtil::UniqFileName();

            FileUtil::WriteFile(PathUtil::Src(file_name),code); //形成临时src文件

            Compiler::Compile(file_name);
            
            Runner::Run(file_name,cpu_limit,mem_limit);

        }
    };
}