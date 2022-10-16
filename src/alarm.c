#include "alarm.h"

#define _POSIX_SOURCE 1 // POSIX compliant source

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("\nAlarm #%d\n", alarmCount);
}

//Starts the alarm
int startAlarm(int timeout)
{
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    if (alarmEnabled == FALSE)
    {
        alarm(timeout);
        alarmEnabled = TRUE;
    }
    

    return 0;
}
