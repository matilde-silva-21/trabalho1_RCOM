#ifndef _ALARM_H_
#define _ALARM_H_


#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal);

int startAlarm(int timeout);

#endif // _ALARM_H_

