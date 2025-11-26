#include "compile_run.hpp"
#include "../comm/httplib.h"

using namespace ns_compile_and_run;
using namespace httplib;

// 编译服务随时可能被多个人请求，必须保证传递上来的code，形成源文件名称的时候，
// 要具有唯一性，不然多个用户之间会相互影响
void Usage(std::string proc)
{
    std::cerr << "Usage: "<<"\n\t"<<proc<<" port"<<std::endl;
}

// ./compile_server port
int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        Usage(argv[0]);
        return 1;
    }

    Server svr;

    svr.Get("/hello", [](const Request &req, Response &rsp)
            { rsp.set_content("hello http,你好", "text/plain;charset=utf-8"); });

    svr.Post("/compile_and_run", [](const Request &req, Response &rsp) 
    {
        std::string in_json = req.body;
        std::string out_json;
        if(!in_json.empty())
        {
            CompileAndRun::Start(in_json,&out_json);
            rsp.set_content(out_json,"application/json;charset=utf-8");
        }
    });

    svr.listen("0.0.0.0", std::stoi(argv[1]));

    // 通过http 让client给我们上传一个json串
    // in_json:{"code":"#include...","input":"","cpu_limit":1,"mem_limit":10240}
    // out_json:{"status":"0","reason":"","stdout":"","stderr":""}
    // 测试
    // std::string in_json;
    // Json::Value in_value;
    // in_value["code"] = R"(#include <iostream>
    // int main()
    // {
    // std::cout<<"模块测试"<<std::endl;
    // int a=10;
    // a/=0;
    // return 0;
    // })";
    // in_value["input"] = "";
    // in_value["cpu_limit"] = 1;
    // in_value["mem_limit"] = 10240 * 3;
    // Json::FastWriter writer;
    // in_json = writer.write(in_value);
    // //std::cout<<in_json<<std::endl;

    // std::string out_json;
    // CompileAndRun::Start(in_json,&out_json);
    // std::cout<<out_json<<std::endl;

    // return 0;
}
