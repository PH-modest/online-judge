#pragma once
#include <iostream>
#include <string>
#include <ctemplate/template.h>

#include "oj_model.hpp"

namespace ns_view
{
    const std::string template_path = "./template_html/";

    class View
    {
    public:
        View(){};
        ~View(){};
        void AllExpandHtml(const std::vector<ns_model::Question> &questions, std::string *html)
        {
            // 题目的编号 题目的标题 题目的难度
            // 形成路径
            std::string src_html = template_path + "all_questions.html";
            // 形成数字典
            ctemplate::TemplateDictionary root("all_questions");
            for(const auto& q : questions)
            {
                ctemplate::TemplateDictionary *sub = root.AddSectionDictionary("question_list");
                sub->SetValue("number",q.number);
                sub->SetValue("title",q.title);
                sub->SetValue("star",q.star);
            }
            //获取被渲染的网页
            ctemplate::Template *tpl = ctemplate::Template::GetTemplate(src_html,ctemplate::DO_NOT_STRIP);
            //开始渲染
            tpl->Expand(html,&root);
        }

        void OneExpandHtml(const ns_model::Question& question,std::string *html)
        {
            
        }
    };
}