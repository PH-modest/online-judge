#pragma once
#include <iostream>
#include <string>
#include <ctemplate/template.h>

// #include "oj_model.hpp"
#include "oj_model_sql.hpp"

namespace ns_view
{
    const std::string template_path = "./template_html/";

    class View
    {
    public:
        View() {};
        ~View() {};
        void AllExpandHtml(const std::vector<ns_model::Question> &questions, const std::unordered_map<std::string, int> &states, std::string *html)
        {
            // 题目的编号 题目的标题 题目的难度
            // 形成路径
            std::string src_html = template_path + "all_questions.html";
            // 形成数字典
            ctemplate::TemplateDictionary root("all_questions");
            for (const auto &q : questions)
            {
                ctemplate::TemplateDictionary *sub = root.AddSectionDictionary("question_list");
                sub->SetValue("number", q.number);
                sub->SetValue("title", q.title);
                sub->SetValue("star", q.star);

                double rate = q.submit_count == 0 ? 0.0 : (double)q.pass_count / q.submit_count * 100.0;
                char rate_str[32];
                snprintf(rate_str, sizeof(rate_str), "%.2f%%", rate);
                sub->SetValue("pass_rate", rate_str);

                auto it = states.find(q.number);
                if (it != states.end())
                {
                    if (it->second == 1)
                    {
                        sub->SetValue("status_icon", "✅");
                    }
                    else
                    {
                        sub->SetValue("status_icon", "❌");
                    }
                }
                else
                {
                    sub->SetValue("status_icon", "-"); // 没有记录说明未尝试
                }
            }
            // 获取被渲染的网页
            ctemplate::Template *tpl = ctemplate::Template::GetTemplate(src_html, ctemplate::DO_NOT_STRIP);
            // 开始渲染
            tpl->Expand(html, &root);
        }

        void OneExpandHtml(const ns_model::Question &question, std::string *html)
        {
            // 形成路径
            std::string src_html = template_path + "one_question.html";
            // 形成数据字典
            ctemplate::TemplateDictionary root("one_question");
            root.SetValue("number", question.number);
            root.SetValue("title", question.title);
            root.SetValue("star", question.star);
            root.SetValue("desc", question.desc);
            root.SetValue("pre_code", question.header);
            // 获取网页
            ctemplate::Template *tpl = ctemplate::Template::GetTemplate(src_html, ctemplate::DO_NOT_STRIP);
            // 开始完成渲染功能
            tpl->Expand(html, &root);
        }
    };
}