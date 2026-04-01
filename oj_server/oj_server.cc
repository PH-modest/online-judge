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
        int user_id = -1;
        // 检查是否有 Authorization 头
        if (req.has_header("Authorization")) {
            std::string token = req.get_header_value("Authorization");
            int role = 0;
            std::string username = "";
            // 验证 Token
            if (ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
                // Token 有效，使用获取到的 user_id
            }
        }
        ctrl.AllQuestions(&html, user_id);
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
        ctrl.Judge(number, req.body, &result_json, user_id);

        Json::Reader reader;
        Json::Value judge_result;
        if(reader.parse(result_json, judge_result))
        {
            int status = judge_result["status"].asInt();
            bool is_passed = (status == 0);
            
            user_model.UpdataUserSubmitState(user_id, is_passed);
            // 更新题目状态
            ctrl.UpdateStats(user_id, number, is_passed);
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

    // 管理员录入新题目与测试用例的接口
    svr.Post("/admin/add_question", [&ctrl](const Request &req, Response &rsp)
    {
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 1. JWT 鉴权与身份识别
        if (!req.has_header("Authorization"))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，无权操作！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0;
        std::string username = "";

        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录凭证已过期或无效，请重新登录。";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 2. 核心鉴权：检验角色是否为管理员 (role == 1)
        if (role != 1) 
        {
            LOG(WARNING) << "越权访问拦截: 普通用户 [" << username << "] 试图调用录题接口！\n";
            rsp_json["status"] = -3;
            rsp_json["reason"] = "权限不足，仅管理员可执行录题操作！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 3. 解析前端传来的 JSON 题目数据
        Json::Reader reader;
        Json::Value req_json;
        if (reader.parse(req.body, req_json))
        {
            ns_model::Question q;
            q.title = req_json["title"].asString();
            q.star = req_json["star"].asString();
            q.desc = req_json["desc"].asString();
            q.header = req_json["header"].asString();
            q.tail = req_json["tail"].asString();
            // 如果前端没有传 cpu/mem limit，给默认值防止解析失败
            q.cpu_limit = req_json.isMember("cpu_limit") ? req_json["cpu_limit"].asInt() : 1;
            q.mem_limit = req_json.isMember("mem_limit") ? req_json["mem_limit"].asInt() : 50000;

            // 4. 调用 Control 层入库
            if (ctrl.AddQuestion(q))
            {
                LOG(INFO) << "管理员 [" << username << "] 录入新题目: " << q.title << " 成功！\n";
                rsp_json["status"] = 0;
                rsp_json["reason"] = "录题成功！";
            }
            else
            {
                rsp_json["status"] = -1;
                rsp_json["reason"] = "数据库录入失败，请检查数据格式或联系服务器管理员。";
            }
        }
        else
        {
            rsp_json["status"] = -1;
            rsp_json["reason"] = "JSON 数据解析失败，请检查请求体格式。";
        }

        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
    });

    // 获取特定题目的历史提交记录接口
    svr.Get(R"(/history/(\d+))", [&ctrl](const Request &req, Response &rsp)
    {
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 拦截器：必须登录才能看历史记录
        if (!req.has_header("Authorization"))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，无法获取历史记录！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0;
        std::string username = "";

        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "凭证无效或已过期！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 提取题号，并获取历史记录
        std::string number = req.matches[1];
        std::string result_json;
        
        if (ctrl.GetHistory(user_id, number, &result_json))
        {
            // 成功：直接返回 JSON 数组
            rsp.set_content(result_json, "application/json;charset=utf-8");
        }
        else
        {
            // 失败：返回错误信息
            rsp_json["status"] = -1;
            rsp_json["reason"] = "数据库查询失败";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
        }
    });

    svr.set_base_dir("./wwwroot");
    svr.listen("0.0.0.0", 8102);
    return 0;
}