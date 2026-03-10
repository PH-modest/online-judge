#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <jwt-cpp/jwt.h>

namespace ns_util
{
    class JwtUtil
    {
    private:
        // 签名私钥
        static const std::string SECRET_KEY;

    public:
        // 生成 Token，签发给登录成功的用户
        static std::string GenerateToken(int id, const std::string& username, int role) 
        {
            auto token = jwt::create()
                .set_issuer("oj_system") // 签发者
                .set_type("JWS")
                .set_payload_claim("id", jwt::claim(std::to_string(id)))
                .set_payload_claim("username", jwt::claim(username))
                .set_payload_claim("role", jwt::claim(std::to_string(role)))
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24)) // 24小时后过期
                .sign(jwt::algorithm::hs256{SECRET_KEY});
            return token;
        }

        // 校验 Token 是否合法、是否过期，并解析出用户信息
        static bool VerifyToken(const std::string& token, int* id, std::string* username, int* role) 
        {
            try 
            {
                auto decoded = jwt::decode(token);
                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{SECRET_KEY})
                    .with_issuer("oj_system");
                
                // 进行校验，如果过期或被篡改，这里会抛出异常
                verifier.verify(decoded);

                // 取出数据
                *id = std::stoi(decoded.get_payload_claim("id").as_string());
                *username = decoded.get_payload_claim("username").as_string();
                *role = std::stoi(decoded.get_payload_claim("role").as_string());
                return true;
            } 
            catch (const std::exception& e) 
            {
                // 校验失败
                return false;
            }
        }
    };
    // 初始化密钥
    const std::string JwtUtil::SECRET_KEY = "Hph_Graduation_Project_666";
}
