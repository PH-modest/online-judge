#include <iostream>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <jsoncpp/json/json.h>

int main()
{
    
    return 0;
}


// int main()
// {
//     //资源不足，导致OS终止进程，是通过信号终止的

//     // //限制累计运行时长
//     // struct rlimit r;
//     // r.rlim_cur = 1;
//     // r.rlim_max = RLIM_INFINITY;
//     // setrlimit(RLIMIT_CPU,&r);
//     // while(1);

//     struct rlimit r;
//     r.rlim_cur = 1024*1024*40;
//     r.rlim_max = RLIM_INFINITY;
//     setrlimit(RLIMIT_AS,&r);

//     int count=0;
//     while(true)
//     {
//         int *p = new int[1024*1024];
//         count++;
//         std::cout<<"size:"<<count<<"\n";
//         sleep(1);
//     }
//     return 0;
// }