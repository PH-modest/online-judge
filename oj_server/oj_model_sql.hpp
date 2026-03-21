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
                q.cpu_limit = atoi(row[6]);
                q.mem_limit = atoi(row[7]);

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
            "ON DUPLICATE KEY UPDATE state = GREATEST(state, VALUES(state))", 
            user_id, number.c_str(), state);

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

        ~Model()
        {
        }
    };
}
