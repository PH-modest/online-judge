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
            // 只要提交了，我们就增加 submit_count。
            // 真实的 pass_count 已经在排行榜通过子查询精确统计了，避免这里重复累加产生脏数据。
            std::string sql = "UPDATE oj_users SET submit_count = submit_count + 1 WHERE id = " + std::to_string(user_id);

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
            // std::string sql = "SELECT id, username, role, pass_count, submit_count FROM oj_users ORDER BY pass_count DESC, submit_count ASC LIMIT " + std::to_string(limit);

            // 使用子查询实时统计该用户在 oj_user_question_status 表中 state=1 的唯一记录数
            std::string sql =
                "SELECT u.id, u.username, u.role, "
                "(SELECT COUNT(*) FROM oj_user_question_status s WHERE s.user_id = u.id AND s.state = 1) AS true_pass_count, "
                "u.submit_count "
                "FROM oj_users u "
                "ORDER BY true_pass_count DESC, u.submit_count ASC LIMIT " +
                std::to_string(limit);

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

        // 写入邮箱验证码
        bool RecordAuthCode(const std::string &email, const std::string &code, const std::string &type)
        {
            std::string del_sql = "DELETE FROM oj_auth_codes WHERE email='" + email + "' AND type='" + type + "'";
            mysql_query(_conn, del_sql.c_str()); // 删除旧的同类型验证码

            std::string ins_sql = "INSERT INTO oj_auth_codes (email, code, type, expire_time) VALUES ('" +
                                  email + "', '" + code + "', '" + type + "', DATE_ADD(NOW(), INTERVAL 5 MINUTE))";

            if (mysql_query(_conn, ins_sql.c_str()) != 0)
            {
                LOG(WARNING) << "写入验证码记录失败: " << mysql_error(_conn) << "\n";
                return false;
            }
            return true;
        }

        // 校验验证码是否匹配且未过期
        bool CheckAuthCode(const std::string &email, const std::string &code, const std::string &type)
        {
            std::string sql = "SELECT id FROM oj_auth_codes WHERE email='" + email +
                              "' AND code='" + code +
                              "' AND type='" + type +
                              "' AND expire_time > NOW()";

            if (mysql_query(_conn, sql.c_str()) != 0) return false;

            MYSQL_RES *res = mysql_store_result(_conn);
            if (res == nullptr) return false;

            int num_rows = mysql_num_rows(res);
            mysql_free_result(res);
            return num_rows > 0;
        }

        // 找回密码：通过邮箱更新密码
        bool UpdatePasswordByEmail(const std::string &email, const std::string &new_password)
        {
            std::string sql = "UPDATE oj_users SET password='" + new_password + "' WHERE email='" + email + "'";
            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "重置密码失败: " << mysql_error(_conn) << "\n";
                return false;
            }
            // 确保真的有行被更新（即邮箱确实存在于系统中）
            return mysql_affected_rows(_conn) > 0;
        }

        // 重载带邮箱的注册方法
        bool RegisterWithEmail(const std::string &username, const std::string &password, const std::string &email)
        {
            std::string sql = "INSERT INTO oj_users (username, password, email, role) VALUES ('" +
                              username + "', '" + password + "', '" + email + "', 0)";

            if (mysql_query(_conn, sql.c_str()) != 0)
            {
                LOG(WARNING) << "用户注册失败 (用户名或邮箱已存在):" << mysql_error(_conn) << "\n";
                return false;
            }
            LOG(INFO) << "新用户注册成功: " << username << " 邮箱: " << email << "\n";
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