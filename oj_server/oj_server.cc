#include <iostream>
#include <signal.h>
#include <jsoncpp/json/json.h>
#include "../comm/log.hpp"
#include "../comm/httplib.h"
#include "../comm/jwt_util.hpp"
#include "oj_control.hpp"
#include "user_model.hpp"

using namespace httplib;
using namespace ns_log;

static ns_control::Control *ctrl_ptr = nullptr;

void Recovery(int signo)
{
    ctrl_ptr->RecoveryMachine();
}

int main()
{
    // 数据库测试
    /*ns_model::UserModel user_model;

    // 1. 测试登录我们在数据库里手动插入的 admin 账号
    ns_model::User u;
    if (user_model.Login("admin", "123456", &u)) {
        std::cout << "测试通过：读取到管理员 ID=" << u.id << std::endl;
    }*/
    // 测试结束

    signal(SIGQUIT, Recovery);
    // 用户请求的服务路由功能
    Server svr;
    ns_control::Control ctrl;
    ctrl_ptr = &ctrl;
    ns_model::UserModel user_model; // 数据库交互类

    // 用户注册接口
    svr.Post("/register", [&user_model](const Request &req, Response &rsp)
    {
        Json::Reader reader;
        Json::Value req_json;
        Json::Value rsp_json;
        
        if(reader.parse(req.body, req_json)) 
        {
            std::string username = req_json["username"].asString();
            std::string password = req_json["password"].asString();
            
            if(user_model.Register(username, password)) {
                rsp_json["status"] = 0;
                rsp_json["reason"] = "注册成功";
            } else {
                rsp_json["status"] = -1;
                rsp_json["reason"] = "注册失败，用户名可能已存在";
            }
        }
        Json::FastWriter writer;
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); 
    });

    // 用户登录接口
    svr.Post("/login", [&user_model](const Request &req, Response &rsp)
    {
        Json::Reader reader;
        Json::Value req_json;
        Json::Value rsp_json;
        
        if(reader.parse(req.body, req_json)) 
        {
            std::string username = req_json["username"].asString();
            std::string password = req_json["password"].asString();
            
            ns_model::User u;
            if(user_model.Login(username, password, &u)) 
            {
                rsp_json["status"] = 0;
                rsp_json["reason"] = "登录成功";
                // 登录成功，生成 JWT 令牌给前端！
                rsp_json["token"] = ns_util::JwtUtil::GenerateToken(u.id, u.username, u.role);
                rsp_json["role"] = u.role; // 把角色也告诉前端，方便前端决定是否显示管理员按钮
            } 
            else 
            {
                rsp_json["status"] = -1;
                rsp_json["reason"] = "用户名或密码错误";
            }
        }
        Json::FastWriter writer;
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); 
    });

    // 获取所有题目列表
    svr.Get("/all_questions", [&ctrl](const Request &req, Response &rsp)
    { 
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

        rsp.set_content(html,"text/html;charset=utf-8"); 
    });

    // 用户提交代码，使用我们的判题功能
    svr.Post(R"(/judge/(\d+))", [&ctrl, &user_model](const Request &req, Response &rsp)
    {
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 权限校验拦截器：检查 HTTP 头里有没有 Authorization 字段
        if (!req.has_header("Authorization"))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，无权提交代码！请先登录。";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0;
        std::string username = "";

        // 验证 Token 是否是被伪造的或者过期了
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录凭证已过期或无效，请重新登录。";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 校验通过，是合法登录用户
        LOG(INFO) << "用户 [" << username << "] 正在提交题目判题请求...\n";

        std::string number = req.matches[1];
        std::string result_json;
        ctrl.Judge(number, req.body, &result_json);

        Json::Reader reader;
        Json::Value judge_result;
        if(reader.parse(result_json, judge_result))
        {
            int status = judge_result["status"].asInt();
            bool is_passed = (status == 0);
            user_model.UpdataUserSubmitState(user_id, is_passed);
        }

        rsp.set_content(result_json, "application/json;charset=utf-8");
        // rsp.set_content("指定题目判题："+number,"text/plain;charset=utf-8");
    });

    svr.Get("/leaderboard", [&user_model](const Request &req, Response &rsp)
    {
        std::vector<ns_model::User> board;
        Json::Value rsp_json;
        Json::Value user_list(Json::arrayValue);

        if (user_model.GetGlobalLeaderboard(&board)) 
        {
            rsp_json["status"] = 0;
            rsp_json["reason"] = "获取排行榜成功";
            for (const auto& u : board) 
            {
                Json::Value user_json;
                user_json["username"] = u.username;
                user_json["pass_count"] = u.pass_count;
                user_json["submit_count"] = u.submit_count;
                // 计算通过率
                double pass_rate = u.submit_count == 0 ? 0.0 : (double)u.pass_count / u.submit_count * 100.0;
                user_json["pass_rate"] = pass_rate;
                user_list.append(user_json);
            }
            rsp_json["data"] = user_list;
        } 
        else 
        {
            rsp_json["status"] = -1;
            rsp_json["reason"] = "获取排行榜失败";
        }

        Json::FastWriter writer;
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); 
    });

    svr.set_base_dir("./wwwroot");
    svr.listen("0.0.0.0", 8102);
    return 0;
}