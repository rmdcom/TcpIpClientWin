/******************************************************************************
*
*  TCP client for testing Linux SYS6000 HMS Transparent Ethernet
*
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
#define DEFAULT_SERVER_IP   "192.168.30.32"
#define PORT_ECHO     7                     /* well known echo port */

#define MAX_TCP_SIZE  (2000)

#define TEST_BLOCK_CNT  (0x1000)     // 4k U32 [U32] (Default), = 16k byte

typedef unsigned long       U32;
typedef unsigned __int64    U64 ;

static const char szProgName[] = "TCPclient";
static const char szVersion[] = "25 Jan 2022";


/******************************************************************************
* Global variables
******************************************************************************/
static char recvbuf[MAX_TCP_SIZE];

static int nErrors ;

static unsigned rxCntU32 = TEST_BLOCK_CNT;



/*----------------------------------------------------------------------------
 * Windows error code text.
 */
static const char *winErrText(int errcode)
{
    const char *errName = "";            // Not listed.
    switch (errcode)
    {
    case WSAEFAULT:
        errName = "WSAEFAULT, ";
        break;
    case WSAECONNABORTED:
        errName = "WSAECONNABORTED, ";
        break;
    case WSAECONNRESET:
        errName = "WSAECONNRESET, ";
        break;
    default:
        break;
    }
    return errName;
}


/*----------------------------------------------------------------------------
 * Connect to the TCP server.
 */
static int connectToServer(const char *destIPStr, 
                           unsigned short portNum)
{
    SOCKET sd = INVALID_SOCKET;
    char errMsg[200];
    errMsg[0] = 0;

    struct sockaddr_in addr;
    addr.sin_family      =  PF_INET;
    addr.sin_port        =  htons( portNum );
    addr.sin_addr.s_addr = inet_addr(destIPStr);
    if(addr.sin_addr.s_addr == INADDR_NONE)
    {
        sprintf(errMsg, "Invalid IP address, %s", destIPStr);
    }
    else
    {
        sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sd == INVALID_SOCKET )
        {
            sprintf(errMsg,  "Socket open failed: %d", WSAGetLastError());
        }
        /*--------------------------------------------------------------------
         * Establish a connection to the specified TCP server
         */
        else
        {
            printf("%s: Connecting to %s\n", szProgName, destIPStr);
            if (connect( sd, 
                          (const struct sockaddr *) &addr,
                          sizeof(struct sockaddr_in))
                   == SOCKET_ERROR)
            {
                int error = WSAGetLastError() ;
                if (error == WSAECONNREFUSED )  //connection refused from server host
                {
                    sprintf(errMsg, "Connection refused from %s", destIPStr);
                }
                else if (error == WSAETIMEDOUT)
                {
                    sprintf(errMsg, "Time out at connect(%s) call", destIPStr);
                }
                else
                {
                    sprintf(errMsg, "Socket connect failed: %d", error);
                }
            }
            else
            {
                ///////////////////////////////////////////////////
                // Set socket to non-blocking mode.
                ///////////////////////////////////////////////////
                unsigned long arg = 22 ;
                if (ioctlsocket (sd, FIONBIO, &arg)  != 0)
                {
                    sprintf(errMsg, "ioctlsocket() failed: %d\n",
                            WSAGetLastError());
                }
                else
                {
                    // Success.
                    printf("%s: Connected with %s\n", szProgName, destIPStr);
                }
                unsigned bufSize = rxCntU32 * sizeof(DWORD) + 128;
                int result = setsockopt(sd,
                                        SOL_SOCKET,         // level
                                        SO_RCVBUF,          // optname
                                        (char *)&bufSize,
                                        sizeof(bufSize));
                if (SOCKET_ERROR == result)
                {
                    printf("setsockopt(SO_RCVBUF, %u) failed, errno %d\n",
                            bufSize, WSAGetLastError());
                }
            }
        }
    }
    if (errMsg[0] != 0)             // Problem?
    {
        printf("%s: %s\n", szProgName, errMsg);
        if (sd != INVALID_SOCKET)
        {
            closesocket(sd);
            sd = INVALID_SOCKET;
        }
    }
    return sd;
}   // connectToServer();


/*----------------------------------------------------------------------------
 * Monitor the input from the TCP server.
 *
 * Output Parameters:
 *     errMsg       -  Message text
 *     blockRxTime  -  Dwell to receive EXPECTED_RX_BLOCK_CNT
 */
static void recvLoop(SOCKET sd, DWORD seed, char *errMsg, DWORD &blockRxTime)
{
    DWORD beSeed = ntohl(seed);
    errMsg[0] = 0;
    
    /*******************************************************************
    Wait for an answer from the specified echo server
    To prevent a socket overrun, we read all available data from the socket
    *******************************************************************/
    
    DWORD elapsedTime;
    DWORD startTime = GetTickCount() ;
    DWORD lastReportTime = startTime;
    unsigned int totalIn = 0 ;
    unsigned int fillIdx = 0;
    unsigned expectedByteCnt = rxCntU32 * sizeof(DWORD);
    while (totalIn < expectedByteCnt)
    {           
        DWORD now = GetTickCount();
        elapsedTime = now - startTime ;
#define USE_TIMEOUT
#ifdef USE_TIMEOUT
        if (elapsedTime > 1000)
        {
#define RX_TIME_LIMIT  (5000)
            if (elapsedTime > RX_TIME_LIMIT)
            {
                sprintf(errMsg, "Receive timeout after %d sec\n",
                    RX_TIME_LIMIT/1000);
                break ;
            }
        }
#endif // USE_TIMEOUT

        int rxCnt = recv( sd, (char *)&recvbuf[fillIdx], MAX_TCP_SIZE - fillIdx, 0);
        if(rxCnt == SOCKET_ERROR)
        {
            int rxError = WSAGetLastError() ;
            if (rxError != WSAEWOULDBLOCK)
            {
                sprintf(errMsg, "Receive error %s%d",
                        winErrText(rxError), rxError);
                break;
            }
            else
            {
                Sleep(2);
            }
        }
        else if (rxCnt == 0)
        {
            sprintf(errMsg, "Connection lost") ;
            break ;
        }
        else
        {
            totalIn += rxCnt ;
            
            DWORD *scan = (DWORD *) &recvbuf[0];
            DWORD *bufStart = scan;
            bool oneShot = false;
            while (rxCnt > sizeof(DWORD))
            {

                if (*scan != beSeed)
                {
                    sprintf(errMsg, 
                        "Unexpected data at idx %u: was 0x%08X, sb %08X",
                        totalIn, *scan, beSeed);
                    break;
                }
                rxCnt -= sizeof(DWORD);     // Consumed.
                scan++;
            }
            if (errMsg[0] != 0)
            {
                break;              // Scan loop failed.
            }
            // Reach here with rxCnt in range [0..3].
            fillIdx = rxCnt;
            *bufStart = *scan;      // Store final bytes at top.
        }

        /*--------------------------------------------------------------
         *  Allow a keyboard termination of program.
         */
        if (kbhit())
        {
#define ESCAPE_CHAR  (0x1B)
            char ch = getch();
            if ((ch == 'x')||(ch == ESCAPE_CHAR))
            {
                sprintf(errMsg, "User exit command") ;
                break ;
            }
        }
    }//while data available

    blockRxTime = elapsedTime;

}   // recvLoop()


/*----------------------------------------------------------------------------
 * Monitor the input from the TCP server.
 */
static void workSocket(SOCKET sd, char *errMsg)
{
    DWORD seed = 0x555;
    DWORD rxTimeTotal = 0;

    unsigned pass = 0;
    unsigned totalRxU32 = 0;
    while (0 == errMsg[0])
    {
        /**************************************************************/
        //send some data to the specified echo server
        /**************************************************************/
        DWORD beSeed = ntohl(++seed);
        int sendCnt = send( sd, (char *) &beSeed, sizeof(DWORD), 0);
        if (sendCnt != sizeof(DWORD))
        {
            int txError = WSAGetLastError();
            sprintf(errMsg, "send() returns %d, error %s%d",
                    sendCnt, winErrText(txError), txError) ;
            break ;
        }
        
        
       
        /*******************************************************************
         Wait for an answer from the TCP echo server
          Set errMsg if any problem detected.
          Report the blockRxTime.
        *******************************************************************/

        DWORD blockRxTime = 0;  // [ms] Output parameter,   
        recvLoop(sd, seed, errMsg, blockRxTime);

        totalRxU32 += rxCntU32;
        rxTimeTotal += blockRxTime;
        if (  ((pass % 100) == 99)        // [ms]
            &&(blockRxTime != 0)
           )
        {
            U32 rateNow = (rxCntU32 * sizeof(DWORD) * 1000)
                        / blockRxTime;
            
            U32 rateAvg = (U32)( (totalRxU32 * sizeof(DWORD) * (U64)1000)
                                / rxTimeTotal);
            
            printf("%u [ms], Rx rate: %u bytes/sec, Average: %u bytes/sec\n",
                    blockRxTime, rateNow, rateAvg);
        }

        pass++;
        
    }//while(ok)


}   // workSocket()

    
/*-------------------------------------------------------------------------
 *  Main work loop for TCP client program.
 */
static int runClient(const char    *destIPStr, 
                     unsigned short portNum,
                     unsigned int   sendcnt)
{
    int result = 80;
    
    SOCKET sd = connectToServer(destIPStr, portNum) ;
    if (sd != INVALID_SOCKET)
    {
        char errMsg[200];
        errMsg[0] = 0;

        workSocket(sd, errMsg);

        closesocket(sd);
        printf("%s: %s\n", szProgName, errMsg);
    }
    return result;
}

/*--------------------------------------------------------------------------
 *  Show command line help
 */
static void helpPrintOut(void)
{
    printf( "\n"
    " Either the 'X' key or Escape key will terminate the program.\n"
    "\n"
    "Example command line arguments:\n"
    "   10.49.38.60     Dotted decimal IPv4 address of SYS6000 (echo server)\n"
    "   B7000           Echo block size 7000 U32 (default is %u U32)\n"
    "   P8007           Echo server's TCP port number 8007 (default is %u)\n",
    TEST_BLOCK_CNT,
    PORT_ECHO );
}

/******************************************************************************
* main()
******************************************************************************/
int main(int argc, char *argv[])
{
    int retval ;
    WSADATA WSAData;
    char *destIPStr = DEFAULT_SERVER_IP ;
    unsigned portNum = PORT_ECHO ;
    unsigned int sendcnt = 60 ;
    
    printf("%s:  Version %s\n",  szProgName, szVersion);

    if (WSAStartup (MAKEWORD(1,1), &WSAData) != 0) 
    { 
        printf ("WSAStartup failed. Error: %d", WSAGetLastError ()); 
        return 1; 
    }
    
    bool showHelp = false;
    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (isdigit(arg[0]))
        {
            if (strchr(arg, '.') != NULL)       // Looks like a dotted IPv4?
            {
                destIPStr = arg;
            }
        }
        else if (isdigit(arg[1]))
        {
            unsigned number = strtoul(&arg[1], NULL, 0);
#define LOWER_CASE ('A' ^ 'a')
            char firstLetter = arg[0] & ~LOWER_CASE;
            switch (firstLetter)
            {
            case 'B':                   // Block size
                if (number != 0)
                {
                    rxCntU32 = number;
                    printf("Test block size set to 0x%X bytes (=%s U32)\n",
                            number * sizeof(U32), &arg[1]);
                }
                break;

            case 'P':
                portNum = number;
                break;

            default:
                break;
            }
        }
        else
        {
            showHelp = true;
        }
    }
    if (showHelp)
    {
        helpPrintOut();
    }
    else
    {
        printf("\n%s: TCP Server IP %s, Port %d\n",
               szProgName, destIPStr, portNum);

        retval = runClient( destIPStr, portNum, sendcnt ) ;

        printf("%s: Program exit\n", szProgName);
        Sleep(1000);                            // Allow console reading.
    }
    return retval ;

}   // main()

// End of file
