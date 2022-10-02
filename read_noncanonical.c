// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

volatile int STOP = FALSE;

enum STATE {
    STATE0,
    STATE1,
    STATE2,
    STATE3, 
    STATE4       
} typedef STATE;

/**
 * Extracts a selection of string and return a new string or NULL.
 * It supports both negative and positive indexes.
 */
char *str_slice(char str[], int slice_from, int slice_to)
{
    // if a string is empty, returns nothing
    if (str[0] == '\0')
        return NULL;

    char *buffer;
    size_t str_len, buffer_len;

    // for negative indexes "slice_from" must be less "slice_to"
    if (slice_to < 0 && slice_from < slice_to) {
        str_len = strlen(str);

        // if "slice_to" goes beyond permissible limits
        if (abs(slice_to) > str_len - 1)
            return NULL;

        // if "slice_from" goes beyond permissible limits
        if (abs(slice_from) > str_len)
            slice_from = (-1) * str_len;

        buffer_len = slice_to - slice_from;
        str += (str_len + slice_from);

    // for positive indexes "slice_from" must be more "slice_to"
    } else if (slice_from >= 0 && slice_to > slice_from) {
        str_len = strlen(str);

        // if "slice_from" goes beyond permissible limits
        if (slice_from > str_len - 1)
            return NULL;

        buffer_len = slice_to - slice_from;
        str += slice_from;

    // otherwise, returns NULL
    } else
        return NULL;

    buffer = calloc(buffer_len, sizeof(char));
    strncpy(buffer, str, buffer_len);
    return buffer;
}


int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}, lastByte[BUF_SIZE+1]={0}, word[42], ADDRESS, CONTROL, BCC; // +1: Save space for the final '\0' char

    STATE st = STATE0;
    int byteCounter = 0;
    
    while (STOP == FALSE)
    {
        int bytes = read(fd, buf, BUF_SIZE+1);
        switch (st)
        {
        case STATE0:
            if(buf==0x7E){
                st = STATE1;
                byteCounter++;
                strcpy(lastByte, buf);
                strncat(word, lastByte, bytes);
            }
            break;

        case STATE1:
            if(buf!=0x7E){
                byteCounter++;
                st=STATE2;
                strcpy(lastByte, buf);
                strncat(word, lastByte, bytes);
                if(byteCounter == 2) strcpy(ADDRESS, lastByte);
            }
            break;

        case STATE2:
            if(buf==0x7E){
                st = STATE3;
            }
            byteCounter++;
            strcpy(lastByte, buf);
            strncat(word, lastByte, bytes);
            if(byteCounter==3) strcpy(CONTROL, lastByte);
            else if(byteCounter==4) strcpy(BCC, lastByte);
            break;
        case STATE3:
            if(byteCounter > 5 || ((ADDRESS^CONTROL) != (BCC))){
                st=STATE0;
                memset(word,0,sizeof(word));
                memset(buf,0,sizeof(buf));
                memset(lastByte,0,sizeof(lastByte));
            }
            else{
                st=STATE4;
            }
            break;
        case STATE4:
            if(CONTROL!=0X03){
                st=STATE0;
            }
            else{
                //TO DO: ALARME
                
                //send UA
                int bytes = write(fd, word, strlen(word)+1);
                STOP = TRUE;
            }
            break;
        default:
            break;
        }
    }

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
