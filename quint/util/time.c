#include <sys/time.h>
#include <time.h>

char* getDateTime(time_t ts)
{
    char* datetime = ctime(&ts);
    return datetime[strcspn(datetime, "\r\n")] = '\0';
}

time_t getTimestamp() 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

