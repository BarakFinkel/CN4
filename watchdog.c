#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>

#define server_port 3000
#define buffer_size 128

int main(int argnum, char *argt[])
{
    // printf("Starting watchdog...\n");      // A line to show that the watchdog is running, for debugging purposes.

    signal(SIGPIPE, SIG_IGN); // Helps preventing crashing when closing the socket later on.

    int temp = 0;             // Setting a temporary variable to help check for errors throughout the program.

    int listenSock = -1;
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Setting the listening socket
    if (listenSock == -1)
    {
        printf("Error : Listen socket creation failed.\n"); // if the creation of the socket failed,
        return 0;                                           // print the error and exit main.
    }

    int reuse = 1;                                                                   
    temp = setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));     // uses a previously chosen socketopt if there is one.
    if (temp < 0)
    {
        printf("Error : Failed to set sock options.\n");      // If the socket's reuse failed,
        return 0;                                                                     // print the error and exit main.
    }

    struct sockaddr_in server_address;                  // Using the imported struct of a socket address using the IPv4 module.
    memset(&server_address, 0, sizeof(server_address)); // Resetting it to default values.

    server_address.sin_family = AF_INET;                // Server address is type IPv4.
    server_address.sin_addr.s_addr = INADDR_ANY;        // Get any address that tries to connect.
    server_address.sin_port = htons(server_port);       // Set the server port to the defined 'server_port'.

    temp = bind(listenSock, (struct sockaddr *)&server_address, sizeof(server_address)); // Bind the socket to a port and IP.
    if (temp == -1)
    {
        printf("Error: Binding failed.\n");             // If the binding failed,
        close(listenSock);                              // print the corresponding error, close the socket and exit main.
        return -1;
    }

    temp = listen(listenSock, 3);                       // Start listening, and set the max queue for awaiting client to 3.
    if (temp == -1)
    {
        printf("Error: Listening failed.");             // If listen failed,
        close(listenSock);                              // print the corresponding error, close the socket and exit main.
        return -1;
    }

    // Accept and incoming connection
    struct sockaddr_in client_address;                  // Using the imported struct of a socket address using the IPv4 module.
    socklen_t client_add_len = sizeof(client_address);  // Setting the size of the address to match what's needed.
        
    memset(&client_address, 0, sizeof(client_address));                                       // Resetting address struct to default values.
    client_add_len = sizeof(client_address);                                                  // Setting the size of the address to match what's needed.
    int clientSock = accept(listenSock, (struct sockaddr *)&client_address, &client_add_len); // Accepting the clients request to connect.

    char buffer[buffer_size] = {0};        // Setting the buffer size to the defined 'buffer_size'.

    // We create a timer which will be used to time out the program if the destination takes too long to respond.

    float time = 0;                  // A float that will be used to store the interval.
    struct timeval start, end;       // A struct of type timeval, which will be used to measure the start and end time.

    while(1)
    {
        
        memset(buffer, 0, 128);

        int recvResult = recv(clientSock, buffer, 6, 0);   // Receiving the better_ping request.

        if (recvResult < 0) 
        {
            printf("Error : Receiving failed.\n");         // If receiving failed, print an error and exit main.
            return -1;
        }         
        else if (recvResult == 0) 
        {
            printf("Error : better_ping is closed, nothing to receive.\n");   // If receiving failed, print an error and exit main.
            return -1;
        }
        else if (recvResult != 6)
        {
            printf("Error: Watchdog received a corrupted buffer.\n");
            return -1;
        }

        else if (strcmp(buffer, "start") != 0)
        {
            printf("Error: Invalid request made.\n");
        }
        
        else
        {
            memset(buffer, 0, 5);

            gettimeofday(&start, NULL);

            // We now create a loop which will run until the destination responds, or until the timer reaches 10 seconds.

            while(1)
            { 
                gettimeofday(&end, NULL);

                time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

                temp = recv(clientSock, buffer, 10, 0);       // Receiving the client's request to continue.
                // check errors on recv()
                if (temp < 0) 
                {
                    printf("Error : Receiving failed.\n");    // If receiving failed, print an error and exit main.
                    close(clientSock);
                    close(listenSock);
                    return -1;
                }
                else if (temp == 0) 
                {
                    printf("Error : better_ping is closed, nothing to receive.\n");   // If receiving failed, print an error and exit main.
                    close(clientSock);
                    close(listenSock);
                    return -1;
                }
                else if (temp != 10)
                {
                    printf("Error: watchdog received a corrupted buffer.\n");
                    close(clientSock);
                    close(listenSock);
                    return -1;
                }
                
                // better_ping got a reply. reset timeout clock
                else if ( strcmp(buffer, "got reply") == 0 )
                {
                    memset(buffer, 0, 10);
                    break;
                }
                // better_ping didn't dot respond yet. answer him if to continue or to stop if time is up
                else if ( strcmp(buffer, "continue?") == 0)
                {
                    memset(buffer,0,10);
                
                    // time is up. send no to watchdog inorder to stop program
                    if( time >= 10 )
                    {
                        strcpy(buffer, "no!");
                    }
                    // there is more time - send yes to watchdog inorder to continue 
                    else
                    {
                        strcpy(buffer, "yes");
                    }
                
                    temp = send(clientSock, buffer, strlen(buffer) + 1, 0); // send the answer
                    
                    // check errors on send()
                    if (temp < 0) 
                    {
                        printf("Error : Sending failed.\n");    // If receiving failed, print an error and exit main.
                        close(clientSock);
                        close(listenSock);
                        return -1;
                    } 
                    else if (temp == 0) 
                    {
                        printf("Error : better_ping is closed, nowhere to send to.\n");   // If receiving failed, print an error and exit main.
                        close(clientSock);
                        close(listenSock);
                        return -1;
                    }
                    else if (temp != 4)
                    {
                        printf("Error: client received a corrupted buffer.\n");
                        close(clientSock);
                        close(listenSock);
                        return -1;
                    } 
                    
                    // time is up. close sockets and stop the program 
                    else if ( strcmp(buffer, "no!") == 0 )
                    { 
                        close(clientSock);
                        close(listenSock);
                        return -1;
                    }

                }

                else 
                {
                    printf("watchdog received an invalid request, closing socket.\n");
                    close(clientSock);
                    close(listenSock);
                    return -1;
                }
            }
        }
    }

    close(listenSock);
    close(clientSock);
    return 0;
}    
