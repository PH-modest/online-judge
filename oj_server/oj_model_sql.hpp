#pragma once
// mysql 版本
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <mysql/mysql.h>
#include "../comm/log.hpp"
#include "../comm/util.hpp"

// 根据题目list文件，加载所有的题目信息到内存中
// model：主要用来和数据进行交互，对外提供访问数据的接口

namespace ns_model
{
    using namespace ns_log;

    struct Question
    {
        std::string number; // 题目编号，唯一
        std::string title;  // 题目标题
        std::string star;   // 难度：简单、中等、困难
        std::string desc;   // 题目的描述
        std::string header; // 题目预设的代码
        std::string tail;   // 题目的测试用例，需要和header拼接，形成完整代码
        int cpu_limit;      // 题目的时间要求（S）
        int mem_limit;      // 题目的空间要求（KB）
        int submit_count;   // 题目的总提交次数
        int pass_count;     // 题目的总通过次数 
    };

    const std::string oj_questions = "oj_questions";
    const std::string host = "49.234.42.200";
    const std::string user = "oj_client";
    const std::string passwd = "Hph2003123148..";
    const std::string db = "oj";
    const int port = 3306;
    class Model
    {
    public:
        Model()
        {
        }

        bool QueryMySql(const std::string &sql, std::vector<Question> *out)
        {
            // 创建mysql句柄
            MYSQL *my = mysql_init(nullptr);

            // 连接数据库
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0))
            {
                LOG(FATAL) << "连接数据库失败!" << "\n";
                LOG(DEBUG) << "失败原因: " << mysql_error(my) << "\n";
                mysql_close(my);
                return false;
            }
            // 设置编码格式
            mysql_set_character_set(my, "utf8");

            LOG(INFO) << "连接数据库成功!" << "\n";

            // 执行sql语句
            if (0 != mysql_query(my, sql.c_str()))
            {
                LOG(WARNING) << sql << " execute error!" << "\n";
                mysql_close(my);
                return false;
            }

            // 提取结果
            MYSQL_RES *res = mysql_store_result(my);

            // 分析结果
            int rows = mysql_num_rows(res);   // 获得行数量
            int cols = mysql_num_fields(res); // 获得列数量

            Question q;
            for (int i = 0; i < rows; i++)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                q.number = row[0];
                q.title = row[1];
                q.star = row[2];
                q.desc = row[3];
                q.header = row[4];
                q.tail = row[5];
                q.cpu_limit = row[6] ? atoi(row[6]) : 1;
                q.mem_limit = row[7] ? atoi(row[7]) : 50000;
                q.submit_count = row[8] ? atoi(row[8]) : 0;
                q.pass_count = row[9] ? atoi(row[9]) : 0;

                out->push_back(q);
            }
            // 释放结果空间
            // free(res);
            mysql_free_result(res);
            // 关闭mysql连接
            mysql_close(my);

            return true;
        }

        bool GetAllQuestions(std::vector<Question> *out)
        {
            std::string sql = "select * from ";
            sql += oj_questions;
            return QueryMySql(sql, out);
        }

        bool GetOneQuestion(const std::string &number, Question *q)
        {
            bool res = false;
            std::string sql = "select * from ";
            sql += oj_questions;
            sql += " where number=";
            sql += number;
            std::vector<Question> result;
            if (QueryMySql(sql, &result))
            {
                if (result.size() == 1)
                {
                    *q = result[0];
                    res = true;
                }
            }
            return res;
        }

        // 更新题目的总提交和通过次数
        bool UpdateQuestionCount(const std::string &number, bool is_passed)
        {
            MYSQL *my = mysql_init(nullptr);
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0)) return false;
            mysql_set_character_set(my, "utf8");

            std::string sql1 = "UPDATE oj_questions SET submit_count = submit_count + 1 WHERE number = " + number;
            mysql_query(my,sql1.c_str());
            
            if (is_passed)
            {
                std::string sql2 = "UPDATE oj_questions SET pass_count = pass_count + 1 WHERE number = " + number;
                mysql_query(my,sql2.c_str());
            }
            mysql_close(my);
            return true;
        }

        // 更新用户对某道题的解答状态
        bool UpdateUserQuestionState(int user_id, const std::string& number, bool is_passed)
        {
            int state = is_passed ? 1 : 0;
            MYSQL *my = mysql_init(nullptr);
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0)) return false;
            mysql_set_character_set(my, "utf8");

            // 利用 ON DUPLICATE KEY UPDATE 防止唯一主键冲突，并使用 GREATEST 防止从"已通过(1)"退化为"未通过(0)"
            char sql[512];
            snprintf(sql, sizeof(sql), 
            "INSERT INTO oj_user_question_status (user_id, question_number, state) VALUES (%d, %s, %d) "
            "ON DUPLICATE KEY UPDATE state = GREATEST(state, %d)", 
            user_id, number.c_str(), state, state);

            mysql_query(my, sql);
            mysql_close(my);
            return true;
        }

        // 批量获取某用户所有做过的题目状态
        bool GetUserQuestionStates(int user_id, std::unordered_map<std::string, int>* states)
        {
            MYSQL *my = mysql_init(nullptr);
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0)) return false;
            mysql_set_character_set(my, "utf8");

            std::string sql = "SELECT question_number, state FROM oj_user_question_status WHERE user_id = " + std::to_string(user_id);
            
            if (0 == mysql_query(my, sql.c_str()))
            {
                MYSQL_RES *res = mysql_store_result(my);
                if(res)
                {
                    int rows = mysql_num_rows(res);
                    for(int i = 0; i < rows; ++i)
                    {
                        MYSQL_ROW row = mysql_fetch_row(res);
                        if(row[0] && row[1])
                        {
                            std::string q_num = row[0]; // 查出来转成 string 当 key
                            int state = atoi(row[1]);
                            (*states)[q_num] = state;
                        }
                    }
                    mysql_free_result(res);
                }
            }
            mysql_close(my);
            return true;
        }

        // 管理员录入新题目到数据库
        bool InsertQuestion(const Question &q)
        {
            MYSQL *my = mysql_init(nullptr);
            if (nullptr == mysql_real_connect(my, host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0)) 
                return false;
            mysql_set_character_set(my, "utf8");

            // 代码和描述中包含大量的引号、换行符等特殊字符
            // 如果直接拼接SQL会导致语法错误或SQL注入，必须进行字符转义
            // 转义后长度最大可能是原来的两倍+1
            char* esc_title = new char[q.title.length() * 2 + 1];
            char* esc_desc = new char[q.desc.length() * 2 + 1];
            char* esc_header = new char[q.header.length() * 2 + 1];
            char* esc_tail = new char[q.tail.length() * 2 + 1];

            mysql_real_escape_string(my, esc_title, q.title.c_str(), q.title.length());
            mysql_real_escape_string(my, esc_desc, q.desc.c_str(), q.desc.length());
            mysql_real_escape_string(my, esc_header, q.header.c_str(), q.header.length());
            mysql_real_escape_string(my, esc_tail, q.tail.c_str(), q.tail.length());

            // 查重
            std::string check_sql = "SELECT number FROM oj_questions WHERE title = '" + std::string(esc_title) + "'";
            if (mysql_query(my, check_sql.c_str()) == 0)
            {
                MYSQL_RES *res = mysql_store_result(my);
                if (res)
                {
                    int rows = mysql_num_rows(res);
                    mysql_free_result(res); // 记得释放结果集，防止内存泄漏
                    
                    if (rows > 0) // 说明数据库里已经有这个标题了
                    {
                        LOG(WARNING) << "录题拦截：题目名称 [" << q.title << "] 已存在！\n";
                        
                        // 查重失败，也要记得释放前面 new 出来的内存，防止内存泄漏（答辩亮点）
                        delete[] esc_title;
                        delete[] esc_desc;
                        delete[] esc_header;
                        delete[] esc_tail;
                        mysql_close(my);
                        
                        return false; // 返回 false 告诉 Controller 插入失败
                    }
                }
            }

            // 注意：desc 是 MySQL 的关键字，必须加反引号 `desc` 才能作为列名使用
            std::string sql_query = "INSERT INTO oj_questions (title, star, `desc`, header, tail, cpu_limit, mem_limit) VALUES ('";
            sql_query += esc_title; sql_query += "', '";
            sql_query += q.star; sql_query += "', '";
            sql_query += esc_desc; sql_query += "', '";
            sql_query += esc_header; sql_query += "', '";
            sql_query += esc_tail; sql_query += "', ";
            sql_query += std::to_string(q.cpu_limit); sql_query += ", ";
            sql_query += std::to_string(q.mem_limit); sql_query += ")";

            int res = mysql_query(my, sql_query.c_str());
            if (res != 0) {
                LOG(ERROR) << "录题失败，错误原因: " << mysql_error(my) << "\n";
            }

            // 释放动态分配的内存
            delete[] esc_title;
            delete[] esc_desc;
            delete[] esc_header;
            delete[] esc_tail;

            mysql_close(my);
            return res == 0;
        }

        ~Model()
        {
        }
    };
}
