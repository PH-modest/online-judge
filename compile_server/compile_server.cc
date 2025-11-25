#include "compile_run.hpp"

using namespace ns_compile_and_run;

// 编译服务随时可能被多个人请求，必须保证传递上来的code，形成源文件名称的时候，
// 要具有唯一性，不然多个用户之间会相互影响
int main()
{
    //提供的编译服务，打包形成一个网络服务
    //cpp-httplib


    // 通过http 让client给我们上传一个json串
    //  in_json:{"code":"#include...","input":"","cpu_limit":1,"mem_limit":10240}
    //  out_json:{"status":"0","reason":"","stdout":"","stderr":""}

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
