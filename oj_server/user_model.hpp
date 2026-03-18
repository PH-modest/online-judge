#pragma once
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include "../comm/log.hpp"

namespace ns_model
{
    using namespace ns_log;

    struct User
    {
        int id;
        std::string username;
        std::string password;
        int role; // 0：普通用户 1：管理员
        int pass_count;
        int submit_count;
    };

    class UserModel
    {
    public:
        UserModel()
        {
            Connect();
        }

        ~UserModel()
        {
            if (_conn != nullptr)
            {
                mysql_close(_conn);
            }
        }

        bool Register(const std::string &username, const std::string &password)
        {
            // 后续可以将密码加密后再拼接到SQL中
            std::string sql = "INSERT INTO oj_users (username, password, role) VALUES ('" + username + "', '" + password + "', 0)";

            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "用户注册失败 (可能用户名已存在):" << mysql_error(_conn) << "\n";
                return false;
            }
            LOG(INFO) << "新用户注册成功: " << username << "\n";
            return true;
        }

        // 登录功能
        bool Login(const std::string &username, const std::string &password, User *out_user)
        {
            std::string sql = "SELECT id, username, password, role FROM oj_users WHERE username = '" + username + "'";

            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "执行登录查询失败: " << mysql_error(_conn) << "\n";
                return false;
            }

            // 获取查询结果
            MYSQL_RES *res = mysql_store_result(_conn);
            if (res == nullptr)
            {
                LOG(WARNING) << "获取结果集失败: " << mysql_error(_conn) << "\n";
                return false;
            }

            int num_rows = mysql_num_rows(res);
            if (num_rows == 0)
            {
                LOG(INFO) << "登录失败: 用户 [" << username << "] 不存在.\n";
                mysql_free_result(res);
                return false;
            }

            // 提取这一行的数据
            MYSQL_ROW row = mysql_fetch_row(res);
            std::string db_password = row[2];

            // 校验密码
            if (password != db_password)
            {
                LOG(INFO) << "登录失败: 用户 [" << username << "] 密码错误.\n";
                mysql_free_result(res);
                return false;
            }

            // 密码正确，将数据填充到输出参数 out_user 中
            out_user->id = std::stoi(row[0]);
            out_user->username = row[1];
            out_user->password = row[2];
            out_user->role = std::stoi(row[3]);

            mysql_free_result(res);
            LOG(INFO) << "用户 [" << username << "] 登录成功, 角色为: " << (out_user->role == 1 ? "管理员" : "普通用户") << "\n";
            return true;
        }

        // 更新用户的提交和通过记录
        bool UpdataUserSubmitState(int user_id, bool is_passed)
        {
            std::string sql;
            if (is_passed)
            {
                sql = "UPDATE oj_users SET submit_count = submit_count + 1, pass_count = pass_count + 1 WHERE id = " + std::to_string(user_id);
            }
            else
            {
                sql = "UPDATE oj_users SET submit_count = submit_count + 1 WHERE id = " + std::to_string(user_id);
            }

            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "更新用户题数记录失败: " << mysql_error(_conn) << "\n";
                return false;
            }
            return true;
        }

        // 获取全站前五十的数据排行榜
        bool GetGlobalLeaderboard(std::vector<User> *board, int limit = 50)
        {
            // 按照 通过数降序、提交数升序(通过数相同，提交少的排前面) 排列
            std::string sql = "SELECT id, username, role, pass_count, submit_count FROM oj_users ORDER BY pass_count DESC, submit_count ASC LIMIT " + std::to_string(limit);

            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "获取排行榜数据失败: " << mysql_error(_conn) << "\n";
                return false;
            }

            MYSQL_RES *res = mysql_store_result(_conn);
            if (res == nullptr)
                return false;

            int num_rows = mysql_num_rows(res);
            for (int i = 0; i < num_rows; ++i)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                User u;
                u.id = std::stoi(row[0]);
                u.username = row[1];
                u.role = std::stoi(row[2]);
                u.pass_count = row[3] ? std::stoi(row[3]) : 0;
                u.submit_count = row[4] ? std::stoi(row[4]) : 0;
                board->push_back(u);
            }
            mysql_free_result(res);
            return true;
        }

    private:
        MYSQL *_conn;

        // 初始化并连接数据库
        bool Connect()
        {
            _conn = mysql_init(nullptr);
            if (_conn == nullptr)
            {
                LOG(FATAL) << "初始化 MySQL 句柄失败!\n";
                return false;
            }

            if (nullptr == mysql_real_connect(_conn, "127.0.0.1", "oj_client", "Hph2003123148..", "oj", 3306, nullptr, 0))
            {
                LOG(FATAL) << "连接数据库失败：" << mysql_error(_conn) << "\n";
                return false;
            }

            mysql_set_character_set(_conn, "utf8");
            LOG(INFO) << "成功连接到 MySQL 数据库!\n";
            return true;
        }
    };
}