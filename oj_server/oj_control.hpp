#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <cassert>
#include <jsoncpp/json/json.h>

#include "../comm/log.hpp"
#include "../comm/util.hpp"
#include "../comm/httplib.h"
#include "oj_model.hpp"
#include "oj_view.hpp"

namespace ns_control
{
    using namespace ns_log;
    using namespace ns_util;
    using namespace ns_view;

    // 提供服务的主机
    class Machine
    {
    public:
        Machine()
            : _ip(""), _port(0), _load(0), _mutex(nullptr)
        {
        }
        ~Machine()
        {
        }

        //提升主机负载
        void IncLoad()
        {
            if(_mutex) _mutex->lock();
            ++_load;
            if(_mutex) _mutex->unlock();
        }
        //减少主机负载
        void DecLoad()
        {
            if(_mutex) _mutex->lock();
            --_load;
            if(_mutex) _mutex->unlock();
        }
        //获取主机负载
        uint64_t Load()
        {
            uint64_t tmp = 0; 
            if(_mutex) _mutex->lock();
            tmp = _load;
            if(_mutex) _mutex->unlock();
            return tmp;
        }

    public:
        std::string _ip;    // 编译服务的ip
        int _port;          // 编译服务的端口
        uint64_t _load;     // 编译服务的负载
        std::mutex *_mutex; // mutex是禁止拷贝的，使用指针
    };

    const std::string service_machine = "./conf/service_machine.conf";
    // 负载均衡模块
    class LoadBlance
    {
    public:
        LoadBlance()
        {
            assert(LoadConf(service_machine));
            LOG(INFO) << "加载 " << service_machine << " 成功" << "\n";
        }
        ~LoadBlance()
        {
        }

        bool LoadConf(const std::string &machine_conf)
        {
            std::ifstream in(machine_conf);
            if (!in.is_open())
            {
                LOG(FATAL) << "加载: " << machine_conf << " 失败" << "\n";
                return false;
            }

            std::string line;
            while (std::getline(in, line))
            {
                std::vector<std::string> tokens;
                StringUtil::SplitString(line, &tokens, ":");
                if (tokens.size() != 2)
                {
                    LOG(WARNING) << " 切分 " << line << " 失败 " << "\n";
                    continue;
                }
                Machine m;
                m._ip = tokens[0];
                m._port = std::atoi(tokens[1].c_str());
                m._load = 0;
                m._mutex = new std::mutex();
                _online.push_back(_machines.size());
                _machines.push_back(m);
            }

            in.close();
            return true;
        }
        /// @brief
        /// @param id 输出型参数
        /// @param m 输出型参数
        /// @return
        bool SmartChoice(int *id, Machine **m)
        {
            // 1.使用选择好的主机（更新该主机的负载）
            // 2.我们需要离线该主机
            _mutex.lock();
            // 负载均衡的算法
            // 1. 随机数+hash
            // 2. 轮询+hash
            int online_num = _online.size();
            if (online_num == 0)
            {
                _mutex.unlock();
                LOG(FATAL) << "所有的后端编译主机已经离线！\n";
                return false;
            }
            // 通过遍历找到负载最小的机器
            *id = _online[0];
            *m = &_machines[_online[0]];
            uint64_t min_load = _machines[_online[0]].Load();
            for (int i = 1; i < online_num; i++)
            {
                uint64_t cur_load = _machines[_online[i]].Load();
                if (min_load > cur_load)
                {
                    min_load = cur_load;
                    *id = _online[i];
                    *m = &_machines[_online[i]];
                }
            }
            _mutex.unlock();
            return true;
        }
        void OfflineMachine(const int id)
        {
            _mutex.lock();
            for(auto iter = _online.begin();iter!= _online.end();++iter)
            {
                if(*iter == id)
                {
                    _online.erase(iter);
                    _offline.push_back(id);
                    break;
                }
            }
            _mutex.unlock();
        }

        void OnlineMachine()
        {
            // 我们统一上线 
            _mutex.lock();
            _online.insert(_online.end(),_offline.begin(),_offline.end());
            _offline.erase(_offline.begin(),_offline.end());
            _mutex.unlock();
            LOG(INFO) << "所有主机上线了！\n";
        }

        //for test
        void ShowMachines()
        {
            _mutex.lock();
            std::cout<<"当前在线主机列表:";
            for(auto & id : _online)
            {
                std::cout<<id<<" ";
            }
            std::cout<<"\n";
            std::cout<<"当前离线主机列表:";
            for(auto & id : _offline)
            {
                std::cout<<id<<" ";
            }
            std::cout<<"\n";
            _mutex.unlock();
        }

    private:
        // 可以给我们提供编译服务的主机
        // 每一台主机都有自己的下标，充当当前主机的id
        std::vector<Machine> _machines;
        // 所有在线的主机
        std::vector<int> _online;
        // 所有离线的主机
        std::vector<int> _offline;
        // 保障LoadBlance的数据安全
        std::mutex _mutex;
    };

    // 这是我们核心业务逻辑的控制器
    class Control
    {
    private:
        ns_model::Model _model; // 提供后台数据
        View _view;             // 提供html渲染功能
        LoadBlance _load_blance; // 核心负载均衡器
    public:
        Control()
        {
        }
        ~Control()
        {
        }

        // 根据题目数据构建网页
        bool AllQuestions(std::string *html)
        {
            bool ret = true;
            std::vector<ns_model::Question> all;
            if (_model.GetAllQuestions(&all))
            {
                // 获取题目信息成功，将所有题目数据构建成网页
                _view.AllExpandHtml(all, html);
            }
            else
            {
                *html = "获取题目失败，形成题目列表失败";
                ret = false;
            }
            return ret;
        }

        bool Question(const std::string &number, std::string *html)
        {
            bool ret = true;
            ns_model::Question q;
            if (_model.GetOneQuestion(number, &q))
            {
                _view.OneExpandHtml(q, html);
            }
            else
            {
                *html = "指定题目：" + number + " 不存在!";
                ret = false;
            }
            return ret;
        }
        // id: 100
        // code: #include...
        // input: ""
        void Judge(const std::string &number, const std::string in_json, std::string *out_json)
        {
            // 0. 根据题目编号，直接拿到对应的题目细节
            ns_model::Question q;
            _model.GetOneQuestion(number,&q);
            
            // 1. in_json进行反序列化，得到题目的id、用户提交的代码、input
            Json::Reader reader;
            Json::Value in_value;
            reader.parse(in_json,in_value);

            // 2. 重新拼接用户代码+测试用例代码，形成新的代码
            std::string code = in_value["code"].asString();
            Json::Value compile_value;
            compile_value["input"] = in_value["input"].asString();
            compile_value["code"] = code + q.tail;
            compile_value["cpu_limit"] = q.cpu_limit;
            compile_value["mem_limit"] = q.mem_limit;
            Json::FastWriter writer;
            std::string compile_string  = writer.write(compile_value);

            // 3. 选择负载最低的主机(差错处理)
            // 规则：一直选择，直到主机可用，否则，就是全部挂掉
            while(true)
            {
                int id = 0;
                Machine *m = nullptr;
                if(!_load_blance.SmartChoice(&id,&m))
                {
                    break;
                }
                LOG(INFO) << "选择主机成功,主机id: " << id << "详情: "<< m->_ip<<":"<< m->_port<<"\n";
                // 4. 然后发起http请求，得到结果
                httplib::Client cli(m->_ip,m->_port);
                m->IncLoad();
                if(auto res = cli.Post("/compile_and_run", compile_string, "application/json;charset=utf-8"))
                {
                    // 5. 将结果赋值给out_json
                    if(res->status == 200)
                    {
                        *out_json = res->body;
                        m->DecLoad();
                        LOG(INFO)<<"请求编译和运行服务成功..."<<"\n";
                        break;
                    }
                    m->DecLoad();                    
                }
                else
                {
                    //请求失败
                    LOG(ERROR) << "当前请求的主机id: " << id << "详情: "<< m->_ip<<":"<< m->_port<<" 可能已经离线"<<"\n";
                    m->DecLoad();
                    _load_blance.OfflineMachine(id);
                    _load_blance.ShowMachines(); // 用来调试
                }

            }
        }
    };
}