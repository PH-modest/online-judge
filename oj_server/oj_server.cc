#include <iostream>
#include "../comm/httplib.h"

using namespace httplib;

int main()
{
    // 用户请求的服务路由功能
    Server svr;

    // 获取所有题目列表
    svr.Get("/all_questions", [](const Request &req, Response &rsp)
            { rsp.set_content("这是题目列表", "text/plain;charset=utf-8"); });
    // 用户要根据题目编号，获取题目的内容
    //  /question/10 -> 正则匹配
    svr.Get(R"(/question/(\d+))", [](const Request &req, Response &rsp)
            {
        std::string number = req.matches[1];
        rsp.set_content("这是题："+number,"text/plain;charset=utf-8"); });

    // 用户提交代码，使用我们的判题功能（1.每题的测试用例 2.compile_and_run）
    svr.Get(R"(/judge/(\d+))", [](const Request &req, Response &rsp)
            {
        std::string number = req.matches[1];
        rsp.set_content("指定题目判题："+number,"text/plain;charset=utf-8"); });

    svr.set_base_dir("./wwwroot");
    svr.listen("0.0.0.0", 8102);
    return 0;
}