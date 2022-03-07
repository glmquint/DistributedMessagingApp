#include <sys/time.h>
#include <time.h>
#include <string.h>

char* getDateTime(time_t ts)
{
    char* datetime = ctime(&ts);
    datetime[strcspn(datetime, "\r\n")] = '\0';
    return datetime;
}

time_t getTimestamp() 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

