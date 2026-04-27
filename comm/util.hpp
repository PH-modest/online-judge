#pragma once

#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <atomic>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

namespace ns_util
{
    class HtmlUtil
    {
    public:
        static void Encode(const std::string &html, std::string *out)
        {
            for (char ch : html)
            {
                switch (ch)
                {
                case '<':
                    *out += "&lt;";
                    break;
                case '>':
                    *out += "&gt;";
                    break;
                case '&':
                    *out += "&amp;";
                    break;
                case '\"':
                    *out += "&quot;";
                    break;
                case '\'':
                    *out += "&apos;";
                    break;
                default:
                    *out += ch;
                    break;
                }
            }
        }
    };

    class TimeUtil
    {
    public:
        static std::string GetTimeStamp()
        {
            struct timeval time;
            gettimeofday(&time, nullptr);
            return std::to_string(time.tv_sec);
        }

        // 获取毫秒时间戳
        static std::string GetTimeMS()
        {
            struct timeval time;
            gettimeofday(&time, nullptr);
            return std::to_string(time.tv_sec * 1000 + time.tv_usec / 1000);
        }
    };

    const std::string temp_path = "./temp/";
    class PathUtil
    {
    public:
        static std::string AddSuffix(const std::string &file_name, const std::string &suffix)
        {
            std::string path_name = temp_path;
            path_name += file_name;
            path_name += suffix;
            return path_name;
        }

        // 编译时需要的临时文件
        // 构建源文件完整路径+后缀名
        // test -> ./temp/test.cpp
        static std::string Src(const std::string &file_name)
        {
            return AddSuffix(file_name, ".cpp");
        }
        // 构建可执行文件完整路径+后缀名
        static std::string Exe(const std::string &file_name)
        {
            return AddSuffix(file_name, ".exe");
        }
        static std::string CompileError(const std::string &file_name)
        {
            return AddSuffix(file_name, ".compile_error");
        }

        // 运行时需要的临时文件
        // 构建标准错误完整的路径+后缀名
        static std::string Stderr(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stderr");
        }
        static std::string Stdin(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stdin");
        }
        static std::string Stdout(const std::string &file_name)
        {
            return AddSuffix(file_name, ".stdout");
        }
    };

    class FileUtil
    {
    public:
        static bool IsFileExist(const std::string &file_name)
        {
            struct stat st;
            if (stat(file_name.c_str(), &st) == 0)
                return true;
            else
                return false;
        }

        // 毫秒级时间戳 + 原子性递增唯一值：来保证唯一性
        static std::string UniqFileName()
        {
            std::atomic_uint id(0);
            id++;
            std::string time = TimeUtil::GetTimeMS();
            std::string uniq_id = std::to_string(id);
            return time + "_" + uniq_id;
        }

        static bool WriteFile(const std::string &target, const std::string &content)
        {
            std::ofstream out(target);
            if (!out.is_open())
            {
                return false;
            }
            out.write(content.c_str(), content.size());
            out.close();
            return true;
        }

        static bool ReadFile(const std::string &target, std::string *content, bool keep = false)
        {
            (*content).clear();
            std::ifstream in(target);
            if (!in.is_open())
            {
                return false;
            }
            std::string line;
            while (std::getline(in, line))
            {
                (*content) += line;
                (*content) += (keep ? "\n" : "");
            }
            in.close();
            return true;
        }
    };

    class StringUtil
    {
    public:
        static void SplitString(const std::string &str, std::vector<std::string> *tokens, const std::string &sep)
        {
            boost::split(*tokens, str, boost::is_any_of(sep), boost::algorithm::token_compress_on);
        }
    };

    class AuthUtil
    {
    public:
        // 生成 6 位随机数字验证码
        static std::string GenerateEmailCode()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(100000, 999999);
            return std::to_string(dis(gen));
        }

        // 生成简单的 SVG 图形验证码 
        static void GenerateCaptcha(std::string &out_svg, std::string &out_text)
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1000, 9999);
            out_text = std::to_string(dis(gen));

            // 生成带噪点和干扰线的轻量级 SVG
            std::stringstream svg;
            svg << "<svg width='100' height='40' xmlns='http://www.w3.org/2000/svg'>"
                << "<rect width='100' height='40' fill='#f2f2f2'/>"
                << "<text x='20' y='28' font-family='Arial' font-size='24' fill='#333' font-weight='bold' letter-spacing='5'>"
                << out_text << "</text>"
                << "<line x1='0' y1='20' x2='100' y2='15' stroke='#888' stroke-width='2'/>"
                << "</svg>";
            out_svg = svg.str();
        }

        // 3. 使用 Linux curl 命令发送邮件
        static bool SendEmail(const std::string &to_email, const std::string &code, const std::string &type)
        {
            std::string subject = (type == "register") ? "OJ系统注册验证码" : "OJ系统密码重置验证码";
            std::string body = "您的验证码是: " + code + "，有效期为5分钟。请勿泄露给他人。";

            std::string from_email = "2692602032@qq.com";   
            std::string auth_code = "cclwpcnuzkqwdfhe";  

            // 使用 \\n 在 C++ 字符串中转义出 \n，交给 shell 的 printf 解释，这是最兼容的写法
            std::string mail_data = "From: OJ Admin <" + from_email + ">\\n"
                                    "To: <" + to_email + ">\\n"
                                    "Subject: " + subject + "\\n\\n" + body;

            // 用 printf 通过管道 | 传给 curl 的 -T - (表示从标准输入读取文件流)
            std::string cmd = "printf '" + mail_data + "' | curl -s --url 'smtps://smtp.qq.com:465' "
                              "--ssl-reqd --login-options AUTH=LOGIN "
                              "--mail-from '" + from_email + "' --mail-rcpt '" + to_email + "' "
                              "--user '" + from_email + ":" + auth_code + "' -T -";

            int ret = system(cmd.c_str());
            return ret == 0;
        }
    };

}