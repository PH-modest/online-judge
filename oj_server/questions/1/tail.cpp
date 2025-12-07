#ifndef COMPILE_ONLINE
#include "header.cpp"
#endif

void Test1()
{
    bool ret = Solution().isPalindrome(121);
    if(ret)
    {
        std::cout<<"通过用例1,测试121通过 ... OK!"<<std::endl;
    }
    else
    {
        std::cout<<"没有通过用例1,测试的值为:121"<<std::endl;
    }
}

void Test2()
{
    bool ret = Solution().isPalindrome(-10);
    if(!ret)
    {
        std::cout<<"通过用例2,测试-10通过 ... OK!"<<std::endl;
    }
    else
    {
        std::cout<<"没有通过用例2,测试的值为:-10"<<std::endl;
    }
}

int main()
{
    Test1();
    Test2();
    return 0;
}
