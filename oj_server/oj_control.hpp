#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "../comm/log.hpp"
#include "../comm/util.hpp"
#include "oj_model.hpp"
#include "oj_view.hpp"

namespace ns_control
{
    using namespace ns_log;
    using namespace ns_util;
    using namespace ns_view;

    class Control
    {
    private:
        ns_model::Model _model; // 提供后台数据
        View _view;   // 提供html渲染功能
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
            if (_model.GetOneQuestions(number, &q))
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
    };
}