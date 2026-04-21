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
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

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
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

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
        // 提取 URL 参数中的难度要求
        std::string star = req.has_param("star")?req.get_param_value("star"):"";
        ctrl.AllQuestions(&html, user_id, star);
        rsp.set_content(html, "text/html;charset=utf-8"); });

    // 用户要根据题目编号，获取题目的内容
    //  /question/10 -> 正则匹配
    svr.Get(R"(/question/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        std::string number = req.matches[1];
        std::string html;
        ctrl.Question(number,&html);

        rsp.set_content(html,"text/html;charset=utf-8"); });

    // 用户提交代码，使用我们的判题功能
    svr.Post(R"(/judge/(\d+))", [&ctrl, &user_model](const Request &req, Response &rsp)
             {
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 1. 权限校验拦截器
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

        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role))
        {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录凭证已过期或无效，请重新登录。";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }


        // 3. 正常判题逻辑
        LOG(INFO) << "用户 [" << username << "] 正在提交题目判题请求...\n";
        std::string number = req.matches[1];

        // 解析前端 JSON，提取 assignment_id
        Json::Reader reader_in;
        Json::Value in_value;
        int assignment_id = 0;
        if(reader_in.parse(req.body, in_value)) {
            if(in_value.isMember("assignment_id")) {
                assignment_id = in_value["assignment_id"].asInt();
            }
        }

        // 截止时间拦截逻辑
        if (assignment_id > 0) {
            if (ctrl.IsAssignmentOverdue(assignment_id)) {
                Json::Value err_json;
                err_json["status"] = -4; // 自定义错误码
                err_json["reason"] = "提交失败：该题单已过截止时间！";
                Json::FastWriter w;
                rsp.set_content(w.write(err_json), "application/json;charset=utf-8");
                return; // 直接返回，不调用后面的编译运行服务
            }
        }

        std::string result_json;
        ctrl.Judge(number, req.body, &result_json, user_id, assignment_id);

        Json::Reader out_reader;
        Json::Value judge_result;
        if(out_reader.parse(result_json, judge_result))
        {
            int status = judge_result["status"].asInt();
            bool is_passed = (status == 0);
            
            user_model.UpdataUserSubmitState(user_id, is_passed);
            ctrl.UpdateStats(user_id, number, is_passed);
        }

        rsp.set_content(result_json, "application/json;charset=utf-8"); });

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
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

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

        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

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
        } });

    // ================= 班级管理功能接口 =================

    // 1. 创建班级
    svr.Post("/class/create", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Value rsp_json;
        Json::FastWriter writer;

        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2; rsp_json["reason"] = "未登录，无法创建班级！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2; rsp_json["reason"] = "凭证无效或过期！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        Json::Reader reader;
        Json::Value req_json;
        if (reader.parse(req.body, req_json)) {
            std::string name = req_json["name"].asString();
            std::string join_code;
            if (ctrl.CreateClass(user_id, name, &join_code)) {
                rsp_json["status"] = 0; 
                rsp_json["reason"] = "创建成功"; 
                rsp_json["join_code"] = join_code; // 返回邀请码供前端展示
            } else {
                rsp_json["status"] = -1; 
                rsp_json["reason"] = "创建失败，数据库异常";
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 2. 加入班级
    svr.Post("/class/join", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Value rsp_json;
        Json::FastWriter writer;
        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2; rsp_json["reason"] = "未登录！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2; rsp_json["reason"] = "凭证无效！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        Json::Reader reader;
        Json::Value req_json;
        if (reader.parse(req.body, req_json)) {
            std::string join_code = req_json["join_code"].asString();
            if (ctrl.JoinClass(user_id, join_code)) {
                rsp_json["status"] = 0; rsp_json["reason"] = "成功加入班级！";
            } else {
                rsp_json["status"] = -1; rsp_json["reason"] = "加入失败，邀请码不存在或系统异常";
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 3. 发布题单（包含录入新题或从题库导入）
    svr.Post("/class/problem_set/create", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Value rsp_json;
        Json::FastWriter writer;

        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2; rsp_json["reason"] = "未登录！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2; rsp_json["reason"] = "凭证无效！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        Json::Reader reader;
        Json::Value req_json;
        if (reader.parse(req.body, req_json)) {
            int class_id = req_json["class_id"].asInt();
            std::string title = req_json["title"].asString();
            std::string deadline = req_json["deadline"].asString(); // 支持传 "" 视为不设期限

            // 解析已存在的题目 ID
            std::vector<int> existing_qids;
            Json::Value exist_json = req_json["existing_questions"];
            for (unsigned int i = 0; i < exist_json.size(); ++i) {
                existing_qids.push_back(exist_json[i].asInt());
            }

            // 解析用户此次发布时创建的“新题”
            std::vector<ns_model::Question> new_qs;
            Json::Value new_qs_json = req_json["new_questions"];
            for (unsigned int i = 0; i < new_qs_json.size(); ++i) {
                ns_model::Question q;
                q.title = new_qs_json[i]["title"].asString();
                q.star = new_qs_json[i]["star"].asString();
                q.desc = new_qs_json[i]["desc"].asString();
                q.header = new_qs_json[i]["header"].asString();
                q.tail = new_qs_json[i]["tail"].asString();
                q.cpu_limit = new_qs_json[i].isMember("cpu_limit") ? new_qs_json[i]["cpu_limit"].asInt() : 1;
                q.mem_limit = new_qs_json[i].isMember("mem_limit") ? new_qs_json[i]["mem_limit"].asInt() : 50000;
                new_qs.push_back(q);
            }

            std::string reason;
            if (ctrl.CreateProblemSet(user_id, class_id, title, deadline, existing_qids, new_qs, &reason)) {
                rsp_json["status"] = 0; 
                rsp_json["reason"] = reason;
            } else {
                rsp_json["status"] = -3; 
                rsp_json["reason"] = reason;
            }
        } else {
            rsp_json["status"] = -1; rsp_json["reason"] = "JSON 格式解析失败";
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 前端获取题单详情
    svr.Get(R"(/api/assignment/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 1. 验证是否登录
        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，无权查看题单！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0;
        std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录凭证已过期！";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 2. 获取题单ID并查询详情
        int assign_id = std::stoi(req.matches[1]);
        std::string result_json;
        
        if (ctrl.GetAssignmentDetailWithStatus(user_id, assign_id, &result_json)) {
            rsp.set_content(result_json, "application/json;charset=utf-8");
        } else {
            rsp_json["status"] = -1;
            rsp_json["reason"] = "获取题单失败，题单可能不存在";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
        } });

    // 获取我的班级列表
    svr.Get("/api/class/list", [&ctrl](const Request &req, Response &rsp)
            {
        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            std::string result;
            if(ctrl.GetMyClassesList(user_id, &result)) {
                rsp.set_content(result, "application/json;charset=utf-8");
                return;
            }
        }
        rsp.set_content("{\"status\":-1}", "application/json;charset=utf-8"); });

    // 创建班级
    svr.Post("/api/class/create", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Reader reader;
        Json::Value req_json;
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 1. 鉴权：获取并验证 Token
        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，请先登录后再创建班级";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }
        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录凭证已过期";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 2. 解析班级名称
        if (reader.parse(req.body, req_json)) {
            std::string class_name = req_json["name"].asString();
            if (class_name.empty()) {
                rsp_json["status"] = -1;
                rsp_json["reason"] = "班级名称不能为空";
            } else {
                std::string join_code;
                // 调用 Control 层逻辑
                if (ctrl.CreateClass(user_id, class_name, &join_code)) {
                    rsp_json["status"] = 0;
                    rsp_json["join_code"] = join_code;
                    rsp_json["reason"] = "班级创建成功！班级码为：" + join_code;
                } else {
                    rsp_json["status"] = -1;
                    rsp_json["reason"] = "数据库写入失败";
                }
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 申请加入班级
    svr.Post("/api/class/join", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Reader reader;
        Json::Value req_json;
        Json::Value rsp_json;
        Json::FastWriter writer;

        // 1. 鉴权
        if (!req.has_header("Authorization")) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "未登录，请先登录";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }
        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role)) {
            rsp_json["status"] = -2;
            rsp_json["reason"] = "登录失效";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
            return;
        }

        // 2. 解析邀请码
        if (reader.parse(req.body, req_json)) {
            std::string join_code = req_json["join_code"].asString();
            // 调用之前在 Control 封装的 JoinClass 逻辑
            if (ctrl.JoinClass(user_id, join_code)) {
                rsp_json["status"] = 0;
                rsp_json["reason"] = "申请成功！请等待班级创建者审核。";
            } else {
                rsp_json["status"] = -1;
                rsp_json["reason"] = "加入失败，请检查邀请码是否正确或是否已在班级中";
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 获取某班级下的所有题单
    svr.Get(R"(/api/class/assignments/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
                std::string token = req.get_header_value("Authorization");
                int user_id = 0, role = 0;
                std::string username = "";
                if (ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role))
                {
                    int class_id = std::stoi(req.matches[1]);
                    std::string result;
                    if (ctrl.GetClassAssignmentsList(class_id, &result))
                    {
                        rsp.set_content(result, "application/json;charset=utf-8");
                        return;
                    }
                }
                rsp.set_content("[]", "application/json;charset=utf-8"); // 查询失败或无数据返回空数组
            });

    // 获取待审核名单
    svr.Get(R"(/api/class/audit_list/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        std::string token = req.get_header_value("Authorization");
        int u_id=0, r=0; std::string uname="";
        if (ns_util::JwtUtil::VerifyToken(token, &u_id, &uname, &r)) {
            int class_id = std::stoi(req.matches[1]);
            std::string result;
            if(ctrl.GetPendingMembersList(class_id, &result)) {
                rsp.set_content(result, "application/json;charset=utf-8");
                return;
            }
        }
        rsp.set_content("[]", "application/json;charset=utf-8"); });

    // 操作审核(同意/拒绝)
    svr.Post("/api/class/audit", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Reader reader; Json::Value req_json, rsp_json; Json::FastWriter writer;
        if (!req.has_header("Authorization")) return; // 鉴权略写，实战同上
        if (reader.parse(req.body, req_json)) {
            int class_id = req_json["class_id"].asInt();
            int target_user_id = req_json["user_id"].asInt();
            int action = req_json["action"].asInt(); // 1 同意, 2 拒绝
            if (ctrl.AuditMemberStatus(class_id, target_user_id, action)) {
                rsp_json["status"] = 0; rsp_json["reason"] = "审核操作成功！";
            } else {
                rsp_json["status"] = -1; rsp_json["reason"] = "操作失败";
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 教师发布新题单
    svr.Post("/api/assignment/publish", [&ctrl](const Request &req, Response &rsp)
             {
        Json::Reader reader; Json::Value req_json, rsp_json; Json::FastWriter writer;
        if (!req.has_header("Authorization")) return; 
        if (reader.parse(req.body, req_json)) {
            int class_id = req_json["class_id"].asInt();
            std::string title = req_json["title"].asString();
            std::string deadline = req_json["deadline"].asString(); // 前端需格式化为 YYYY-MM-DD HH:MM:SS
            int type = req_json["type"].asInt();
            
            // 解析前端发来的题目数组 [1, 2, 5]
            std::vector<int> qids;
            for (const auto& q : req_json["questions"]) {
                qids.push_back(q.asInt());
            }

            std::string reason;
            if (ctrl.PublishAssignment(class_id, title, deadline, type, qids, &reason)) {
                rsp_json["status"] = 0; rsp_json["reason"] = "题单发布成功！";
            } else {
                rsp_json["status"] = -1; rsp_json["reason"] = reason;
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); });

    // 获取班级成员名单
    svr.Get(R"(/api/class/members/(\d+))", [&ctrl](const Request &req, Response &rsp)
            {
        std::string token = req.get_header_value("Authorization");
        int u_id=0, r=0; std::string uname="";
        if (ns_util::JwtUtil::VerifyToken(token, &u_id, &uname, &r)) {
            int class_id = std::stoi(req.matches[1]);
            std::string result;
            if(ctrl.GetClassMembersList(class_id, &result)) {
                rsp.set_content(result, "application/json;charset=utf-8");
                return;
            }
        }
        rsp.set_content("[]", "application/json;charset=utf-8"); });

    // 获取全部题目简略列表用于选择器
    svr.Get("/api/question/list", [&ctrl](const Request &req, Response &rsp)
            {
        std::string result;
        if (ctrl.GetAllQuestionsList(&result)) {
            rsp.set_content(result, "application/json;charset=utf-8");
        } else {
            rsp.set_content("[]", "application/json;charset=utf-8");
        } });

    // 获取题目详情用于编辑回填
    svr.Get(R"(/admin/get_question/(\d+))", [&ctrl](const Request &req, Response &rsp) {
        std::string number = req.matches[1];
        std::string json_res;
        if(ctrl.GetQuestionJson(number, &json_res)) {
            rsp.set_content(json_res, "application/json;charset=utf-8");
        } else {
            rsp.set_content("{\"status\":-1}", "application/json;charset=utf-8");
        }
    });

    // 管理员修改题目内容
    svr.Post("/admin/update_question", [&ctrl](const Request &req, Response &rsp) {
        Json::Value rsp_json; Json::FastWriter writer;
        // 1. JWT 鉴权 (逻辑同 add_question)
        std::string token = req.get_header_value("Authorization");
        int user_id = 0, role = 0; std::string username = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &user_id, &username, &role) || role != 1) {
            rsp_json["status"] = -2; rsp_json["reason"] = "权限不足";
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        // 2. 解析 JSON 并更新
        Json::Reader reader; Json::Value req_json;
        if (reader.parse(req.body, req_json)) {
            ns_model::Question q;
            q.number = req_json["number"].asString();
            q.title = req_json["title"].asString();
            q.star = req_json["star"].asString();
            q.desc = req_json["desc"].asString();
            q.header = req_json["header"].asString();
            q.tail = req_json["tail"].asString();
            q.cpu_limit = req_json["cpu_limit"].asInt();
            q.mem_limit = req_json["mem_limit"].asInt();

            if (ctrl.ModifyQuestion(q)) {
                rsp_json["status"] = 0; rsp_json["reason"] = "修改成功";
            } else {
                rsp_json["status"] = -1; rsp_json["reason"] = "修改失败";
            }
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
    });

    // 管理员删除题目
    svr.Post(R"(/admin/delete_question/(\d+))", [&ctrl](const Request &req, Response &rsp) {
        Json::Value rsp_json; Json::FastWriter writer;
        std::string token = req.get_header_value("Authorization");
        int uid = 0, role = 0; std::string uname = "";
        if (!ns_util::JwtUtil::VerifyToken(token, &uid, &uname, &role) || role != 1) {
            rsp_json["status"] = -2;
            rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8"); return;
        }

        std::string number = req.matches[1];
        if (ctrl.RemoveQuestion(number)) {
            rsp_json["status"] = 0;
        } else {
            rsp_json["status"] = -1;
        }
        rsp.set_content(writer.write(rsp_json), "application/json;charset=utf-8");
    });

    svr.set_base_dir("./wwwroot");
    svr.listen("0.0.0.0", 8102);
    return 0;
}