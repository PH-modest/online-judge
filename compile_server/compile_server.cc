#include "compile_run.hpp"

using namespace ns_compile_and_run;

//编译服务随时可能被多个人请求，必须保证传递上来的code，形成源文件名称的时候，
//要具有唯一性，不然多个用户之间会相互影响
int main()
{
    //通过http 让client给我们上传一个json串
    return 0;
}