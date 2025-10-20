#include<unistd.h>
#include<sys/timerfd.h>
#include<time.h>
#include<fcntl.h>
#include<cstring>
#include<cstdio>
#include<cstdint>

int main()
{
    int timerfd = timerfd_create(CLOCK_MONOTONIC,0);
    if(timerfd < 0)
    {
        perror("timerfd_create");
        return 1;
    }

    struct itimerspec itime;
    memset(&itime,0,sizeof(itime));
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0;

    if(timerfd_settime(timerfd,0,&itime,NULL) < 0)
    {
        perror("timerfd_settime");
        return 1;
    }

    while(1)
    {
        uint64_t exp;
        read(timerfd,&exp,sizeof(exp));
        printf("exp = %ld\n",exp);
    }
    return 0;
}