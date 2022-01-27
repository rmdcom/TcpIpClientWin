/******************************************************************************
*
* (c) 2010 by BECK IPC GmbH
*
*******************************************************************************
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include "stdlib.h"
#include "stdio.h"
#include <conio.h>
#include "winsock2.h"

/******************************************************************************
* Constants
******************************************************************************/
#define PORT_ECHO                      7    /*well known echo port*/
#define MAX_UDP_SIZE    2000

#define MAX_SHOW  (16)

/******************************************************************************
* Global variables
******************************************************************************/


static char sendbuf[MAX_UDP_SIZE]; 
static char recvbuf[MAX_UDP_SIZE]; 

extern int nErrors ;

/******************************************************************************
* runUdpClient()
******************************************************************************/
int runUdpClient(const char *destIPStr, unsigned int sendcnt)
{
    BOOL running = FALSE ;
    struct sockaddr_in   addr;
    struct sockaddr_in   raddr;
    char send_data = 'a' ;
    
    int    fromLength=sizeof(struct sockaddr_in);
    int    toLength  =sizeof(struct sockaddr_in);
    
    int result, sd, i, error;
    
    nErrors = 0 ;
    printf("\nUDP Echo Client:\nDest. IP: %s ,  Port %d\r\n",
            destIPStr,
            PORT_ECHO);
    if (sendcnt > MAX_UDP_SIZE)
    {
        printf("\nMAX_UDP_SIZE (%u) exceeded, %u\n",
               MAX_UDP_SIZE, sendcnt) ;
        return 21 ;
    }
    sd = socket( AF_INET,SOCK_DGRAM, IPPROTO_UDP);
    if(sd == -1)
    {
        printf("\r\nSocket open failed %d",WSAGetLastError());
        return 22 ;
    }
    
   
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr( destIPStr); // Set destination IP
    if (addr.sin_addr.s_addr == -1)
    {
        printf("\r\nCannot convert %s",destIPStr);
    }
    else
    {
        ///////////////////////////////////////////////////
        // Set socket to non-blocking mode.
        ///////////////////////////////////////////////////
        unsigned long arg = 22 ;
        result = ioctlsocket (sd, FIONBIO, &arg) ;
        if (result != 0 )
        {
            printf("\nUDP Client: ioctlsocket() failed: %d", 
                    WSAGetLastError());
        }
        else
        {
            running = TRUE ;
        }
    }
    addr.sin_port = htons( PORT_ECHO ); // Set destination port
    
    while (running)   //loop forever
    {
        /**************************************************************/
        //send some data to the specified echo server
        /**************************************************************/
        memset(sendbuf, send_data, sendcnt) ;
        result = sendto( sd, sendbuf, sendcnt, 0,
            (const struct sockaddr *)&addr, toLength) ;
        if (result == SOCKET_ERROR)
        {
            running = FALSE ;
            printf("\nUDP Client: Send error %d", WSAGetLastError()) ;
            break ;
        }
        
        printf("\nUDP Client: Sent %u %c characters:\n",
                sendcnt, send_data);

        /*****************************************************************
        Sleep a little bit
        *****************************************************************/
        Sleep( 500 );
        
        /*******************************************************************
        Wait for echo answers from the specified echo server
        *******************************************************************/
        do
        {
            if (kbhit())
            {
                if (getch() == 'x')
                {
                    printf("\nUser exit command\n") ;
                    running = FALSE ;
                    break ;
                }
            }
            //Non blocking receive
            result = recvfrom( sd, recvbuf, MAX_UDP_SIZE,
                                0, 
                                (struct sockaddr *)&raddr,
                                &fromLength) ;
            if ( result == SOCKET_ERROR )
            {
                error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK)
                {
                    printf("\r\nReceive error: %d ",error);
                    nErrors++ ;
                    running = FALSE ;
                    break ;
                }
            }
            else
            {
                if (result > 0)    //data received
                {
                    printf("\r\nReceived %u byte packet: ", result);
                    for (i=1; i < result; i++)
                    {
                        printf("%c",(char)recvbuf[i]);
                        if (i > MAX_SHOW)
                        {
                            break ;
                        }
                    }
                }
            }
        }while(result > 0) ;

        // Modify the send data
        send_data++ ;
        if (send_data > 'z')
        {
            send_data = 'a' ;       // wrap around.
        }

    }   // while(running)
    
    closesocket( sd) ;
    if (nErrors == 0)
    {
        printf("\nUDP TEST PASSED\n") ;
    }
    else
    {
        printf("\nUDP TEST FAILED\n") ;
    }
    
    return 0;
}

/*************************************************************************/
//end UDPclie.c
/*************************************************************************/
