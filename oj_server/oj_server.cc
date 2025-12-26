#include <iostream>
#include "../comm/httplib.h"
#include "oj_control.hpp"

using namespace httplib;
using namespace ns_control;

int main()
{
    // 用户请求的服务路由功能
    Server svr;
    Control ctrl;

    // 获取所有题目列表
    svr.Get("/all_questions", [&ctrl](const Request &req, Response &rsp){ 
        //返回一张包含所有题目的html网页
        std::string html;
        ctrl.AllQuestions(&html);
        rsp.set_content(html, "text/html;charset=utf-8"); 
    });
    // 用户要根据题目编号，获取题目的内容
    //  /question/10 -> 正则匹配
    svr.Get(R"(/question/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        std::string number = req.matches[1];
        std::string html;
        ctrl.Question(number,&html);

        rsp.set_content(html,"text/html;charset=utf-8"); });

    // 用户提交代码，使用我们的判题功能（1.每题的测试用例 2.compile_and_run）
    svr.Post(R"(/judge/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        std::string number = req.matches[1];
        std::string result_json;
        ctrl.Judge(number,req.body,&result_json);
        rsp.set_content(result_json,"application/json;charset=utf-8");
        //rsp.set_content("指定题目判题："+number,"text/plain;charset=utf-8"); 
        });

    svr.set_base_dir("./wwwroot");
    svr.listen("0.0.0.0", 8102);
    return 0;
}