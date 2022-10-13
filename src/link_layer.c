// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

extern int alarmEnabled, alarmCount;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
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

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
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


    if(connectionParameters.role == LlTx){

        unsigned char buf[5] = {0}, parcels[5] = {0};

        buf[0] = 0x7E;
        buf[1] = 0x03;
        buf[2] = 0x03;
        buf[3] = 0x00;
        buf[4] = 0x7E;


        while(connectionParameters.nRetransmissions < 4){

            if(!alarmEnabled){
                int bytes = write(fd, buf, sizeof(buf));
                printf("\nSET message sent, %d bytes written\n", bytes);
                startAlarm();
            }
            
            int result = read(fd, parcels, 5);
            if(result != -1 && parcels != 0){
                //se o UA estiver errado 
                if(parcels[2] != 0x07 || (parcels[4] != (parcels[2]^parcels[3]))){
                    printf("\nUA not correct: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    return -1;
                }
                
                else{   
                    printf("\nUA correctly received: 0x%02x%02x%02x%02x%02x\n", parcels[0], parcels[1], parcels[2], parcels[3], parcels[4]);
                    break;
                }
            }

        }

        if(alarmCount > 3){
            printf("\nAlarm limit reached, SET message not sent\n");
            return -1;
        }
    }

    else{
        unsigned char buf[1] = {0}, parcels[5] = {0}; // +1: Save space for the final '\0' char

    STATE st = STATE0;
    unsigned char readByte = TRUE;
    
    // Loop for input
    while (STOP == FALSE)
    { 
        if(readByte){
            int bytes = read(fd, buf, 1); //ler byte a byte
            if(bytes==0) continue;
        }
       
        
        switch (st)
        {
        case STATE0:
            if(buf[0] == 0x7E){
                st = STATE1;
                parcels[0] = buf[0];
            }
            break;

        case STATE1:
            if(buf[0] != 0x7E){
                st = STATE2;
                parcels[1] = buf[0];
            }
            else {
                st = STATE0;
                memset(parcels, 0, 5);
            }
            break;

        case STATE2:
            if(buf[0] != 0x7E){
                st = STATE3;
                parcels[2] = buf[0];
            }
            else {
                st = STATE0;
                memset(parcels, 0, 5);
            }
            break;

        case STATE3:
            if(buf[0] != 0x7E){
                parcels[3] = buf[0];
                st = STATE4;
            }
            else {
                st = STATE0;
                memset(parcels, 0, 5);
            }
            break;

        case STATE4:
            if(buf[0] == 0x7E){
                parcels[4] = buf[0];
                st = STATE5;
                readByte = FALSE;
            }

            else {
                st = STATE0;
                memset(parcels, 0, 5);
            }
            break;
        case STATE5:
            if(((parcels[1])^(parcels[2]))==(parcels[3])){
                printf("\nGreat success! SET message received without errors\n\n");
                STOP = TRUE;
            }
            else {
                st = STATE0;
                memset(parcels, 0, 5);
                readByte = TRUE;
            }
            break;
        default:
            break;
        }
    }

    parcels[2] = 0x07;
    parcels[4] = parcels[2]^parcels[3];

    //preciso de estar dentro da state machine ate receber um sinal a dizer que o UA foi corretamente recebido

    int bytes = write(fd, parcels, sizeof(parcels));
    printf("UA message sent, %d bytes written\n", bytes);
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
