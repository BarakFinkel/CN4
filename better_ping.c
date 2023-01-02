#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>    // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define IP4_HDRLEN 20    // IPv4 header len without options
#define ICMP_HDRLEN 8    // ICMP header len for echo messages
#define server_port 3000
#define server_ip "127.0.0.1"
#define buffer_size 128

// # Function Headers #

unsigned short calculate_checksum(unsigned short *paddress, int len);
int makePacket(char *pac);

// To execute the program, run it from the command line with the following syntax: ./ping <destination_ip>

int main(int argnum, char *argt[])
{
    // First, we check for errors regarding the execution of the program:
    
    // If we have an incorrect number of arguments, we print an error message and exit the program.

    if (argnum != 2)
    {
        printf("Invalid number of arguments when executing. Correct usage: ./ping <destination_ip>\n");
        return 0;
    }

    char ip[INET_ADDRSTRLEN];
    strcpy(ip, argt[1]);
    
    struct in_addr pingaddr;


    // If the IP address is not of type IPv4, we print an error message and exit the program.

    if (inet_pton(AF_INET, ip, &pingaddr) != 1)
    {
        printf("Invalid IP address. Please try again.\n");
        return 0;
    }


    // We now create a struct of type sockaddr_in, which will contain the destination IP address.

    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);


    // Next, we create a raw socket which will be used to transfer the ping in ICMP Protocol to the given ip.

    int rawsock = -1;
    if ((rawsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d\n", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n");
        return -1;
    }


    // We now want to set the raw socket to be non-blocking.

    int status = fcntl(rawsock, F_SETFL, fcntl(rawsock, F_GETFL, 0) | O_NONBLOCK);
    if ( status == -1 )
    {
        printf("Setting rawsock to be non-blocking failed, closing program.\n");
        return -1;
    }

    // We now intialize the variables below:

    int seq = 0;                   // We set a sequence counter to 0, which will be used to identify the ICMP packets sent by this program.
    char pac[IP_MAXPACKET];        // We also create a buffer which will contain the ICMP packet.
    float time = 0;                // Lastly, we create a variable which will contain the time it took to get a reply from the destination.

    // We now create variables for a child process which will be used to execute the watchdog program.
    // More on the watchdog program is written in the watchdog.c file.

    char *wdarg[2];                // This array will contain the arguments for the watchdog program.
    wdarg[0] = "./watchdog";       // The watchdog program is called watchdog.
    wdarg[1] = NULL;               // The watchdog program takes no arguments besides its name.
    int exec = 0;                  // This variable will be used to check if the watchdog program was executed successfully.

    // We now create a fork process which will execute the watchdog program:
    // If there is an error creating the child process or executing the watchdog program, we print an error message and exit the program.

    int child = fork();                 // create the child process 
    if (child == 0)     // child process will  execute the watchdog 
    {
        exec = execv(wdarg[0], wdarg);
            
        if (exec == -1)
        {
            printf("Error executing Watchdog program.\n");
            return -1;
        }   
    }

    // If the watchdog program was executed successfully, we continue to send the ICMP packet to the destination.

    else            //  continue from here with the parent process 
    {
        sleep(1);                                               // We use a sleep() method to let Watchdog get to the neccessary stage in order to proceed.

        int temp = 0;                                           // Setting a temporary variable to help check for errors throughout the program.

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   // Creating a socket using the IPv4 module.

        if (sock == -1) {
            printf("Error : Listen socket creation failed.\n"); // If the socket creation failed, print the error and exit main.   
            return -1;
        }

        struct sockaddr_in serverAddress;                       // Using the imported struct of a socket address using the IPv4 module.
        memset(&serverAddress, 0, sizeof(serverAddress));       // Resetting it to default values.

        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(server_port);                                          // Changing little endian to big endian.
        temp = inet_pton(AF_INET, (const char *)server_ip, &serverAddress.sin_addr);          // Converting the IP address from string to binary.

        if (temp <= 0) {
            printf("Error: inet_pton() failed.\n");                                           // If the conversion failed, print the error and exit main.
            return -1;
        }

        // Make a connection to the server with socket SendingSocket.

        int connectResult = connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)); 
        if (connectResult == -1) 
        {
            printf("Error: Connecting to Watchdog failed.\n");   
            close(sock);
            close(rawsock);
            return -1;
        }

        // if we passed until here it means that we succussfully connected to watchdog

        printf("Pinging the address: %s\n", ip);

        char buffer[buffer_size] = {0};             // initialize a buffer for holding messages to watchdog 
        
        // For each packet sent, we track the time of sending it and getting a reply from the destination.
        // We use the gettimeofday() function to get the time in seconds and microseconds.

        struct timeval start, end;

        while(1)
        {
            sleep(1);                                                       // sleep in order to enable more convenient printing
            
            strcpy(buffer,"start");                                 // message for watchdog
            
           //  send Watchdog to start mesuring the timeout clock
            temp = send(sock, buffer, strlen(buffer) + 1, 0);
           
            // checking possible errors of send()
            if (temp < 0) 
            {
                printf("Error : Sending failed.\n");    // If receiving failed, print an error and exit main.
                close(sock);
                close(rawsock);
                return -1;
            } 
            else if (temp == 0) 
            {
                printf("Error : Client's socket is closed, nowhere to send to.\n");   // If receiving failed, print an error and exit main.
                close(sock);
                close(rawsock);
                return -1;
            }
            else if (temp != strlen(buffer) + 1)
            {
                printf("Error: client received a corrupted buffer.\n");
                close(sock);
                close(rawsock);
                return -1;
            }
            
            memset(buffer, 0, strlen(buffer) + 1);                      // reset the buffer. 

            gettimeofday(&start, 0);                                    // start mesuring the times it takes to send a ping and receive the 'pong' message

            // We use the sendto() function to send the packet to the destination.
            
            int len = makePacket(pac);                                  // make the packet header. len indicates on the length of the header

            int temp = sendto(rawsock, pac, len, 0, (struct sockaddr *)&address, sizeof(address));              // send the 'ping' message to distination 
            
            if (temp == -1) // check error on send()
            {
                printf("Sending packet failed with error: %d\n", errno);
                close(sock);
                close(rawsock);
                return -1;
            }

            // We now clear the packet buffer receiving a reply from the destination.

            bzero(pac, IP_MAXPACKET);

            socklen_t addlen = sizeof(address);    

            // We now begin a receiving loop, for getting the reply message for our 'ping'.
            // we are recv with a non-blocking socket, for enabling the communication with the watchdog
            // so the watch dog will be able to notify us if the timeout clock ran out.(10 seconds)

            ssize_t rec = -1; 

            while (1)  
            {
              
                rec = recvfrom(rawsock, pac, sizeof(pac), 0, (struct sockaddr *)&address, &addlen);   // receive 'pong' on non-blocking socket    
                
                // in case we got the message 
                if (rec > 0)                           
                {   
                    // We now get the end time of receiving the reply from the destination, and calculate the time it took to get the reply.

                    gettimeofday(&end, 0);
                    
                    strcpy(buffer, "got reply");

                    temp = send(sock, buffer, strlen(buffer) + 1, 0); // notify the watchdog that we got the message, so he can reset the timeout clock
                    
                    //  checking errors of send() 
                    if (temp < 0) 
                    {
                        printf("Error : Sending failed.\n");    // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    } 
                    else if (temp == 0) 
                    {
                        printf("Error : Watchdog's socket is closed, nowhere to send to.\n");   // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    }
                    else if (temp != strlen(buffer) + 1)
                    {
                        printf("Error: Watchdog received a corrupted buffer.\n");
                        close(sock);
                        close(rawsock);
                        return -1;
                    }
                    
                    // no errors on sending - break and send another ping message
                    
                    break;     // If we got a reply, we break the loop.
                }
                // we didn't got the message yet. check with watch if time ran out
                else
                {
                    strcpy(buffer, "continue?");            // send watch dog a message asking if to continue receiving? 
                    
                    temp = send(sock, buffer, strlen(buffer) + 1, 0);
                    
                    //  check errors of send()
                    if (temp < 0) 
                    {
                        printf("Error : Sending failed.\n");    // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    } 
                    else if (temp == 0) 
                    {
                        printf("Error : Watchdog's socket is closed, nowhere to send to.\n");   // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    }
                    else if (temp != strlen(buffer) + 1)
                    {
                        printf("Error: Watchdog received a corrupted buffer.\n");
                        close(sock);
                        close(rawsock);
                        return -1;
                    }

                    memset(buffer,0,10); // reset the buffer 

                    temp = recv(sock, buffer, 4, 0); // recv answer from watchdog whether to continue
                   
                    // check errors on recv() 
                    if (temp < 0) 
                    {
                        printf("Error : Sending failed.\n");    // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    } 
                    else if (temp == 0) 
                    {
                        printf("Error : Watchdog's socket is closed, nowhere to receive from.\n");   // If receiving failed, print an error and exit main.
                        close(sock);
                        close(rawsock);
                        return -1;
                    }
                    else if (temp != 4)
                    {
                        printf("Error: Received a corrupted buffer.\n");
                        close(sock);
                        close(rawsock);
                        return -1;
                    }

                     
                    // if the answer is no - close socket and quit program 
                    else if ( strcmp (buffer, "no!") == 0 ) 
                    {
                        printf("Time out! Closing socket.\n");
                        close(sock);
                        close(rawsock);
                        return -1;                        
                    }
                    
                    
                    // if answer is yes - continue 
                    else if ( strcmp (buffer, "yes") == 0 ) 
                    {
                        continue;
                    }
                    
                    // there is an invalid response - quit 
                    else 
                    {
                        printf("Invalid response from Watchdog, closing socket.\n");
                        close(sock);
                        close(rawsock);
                        return -1;
                    }

                    memset(buffer,0,4);
                }
            }
            
            // calculate the time of sending and receiving the ping message 
            
            float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
            unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
            time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;

            // We will now print data about the reply we got from the destination.

            printf("-- Reply from %s : seq = %d, bytes = %ld, time = %.3f ms.\n", ip, seq, rec, time);

            // We now increment the sequence counter, and clear the packet buffer for the next iteration of the loop, if any occur.

            bzero(pac, IP_MAXPACKET);
            seq++;
        }
    
        // If we broke the loop, it means we didn't receive a reply propperly.
        // Therefore, we close the socket and exit the program.

        printf("Closing socket, goodbye!.\n");

        close(sock);
        close(rawsock);

        return 0;
    } 
}


// # The Functions #

//// calculate_checksum() - is used for calculating checksum of the packet, which is inserted in the ICMP header.

unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}


//// makePacket() - creates the packet meant to be sent to the destination.

int makePacket(char *pac)
{
    // Creating the parts of the ICMP header and adding it to the packet:
    
    struct icmp header;

    header.icmp_type = ICMP_ECHO;         // Message Type (Consists of 8 bits)      - the type of the message, which in our case is an echo message.
    header.icmp_code = 0;                 // Message Code (Consists of 8 bits)      - 0 represents the package is an echo request.
    header.icmp_id = 24;                  // Message ID (Consists of 16 bits)       - helps the receiver to identify the full message created by the packets.
    header.icmp_seq = 0;                  // Message Sequence (Consists of 16 bits) - keeps track of the numbers and order of packets sent
    header.icmp_cksum = 0;                // Checksum (Consists of 16 bits)         - used to verify the integrity of the packet (Will be calculated later)

    memcpy(pac, &header, ICMP_HDRLEN);    // Copying the ICMP header to the packet.

    // Adding the data to the packet:

    char data[IP_MAXPACKET] = "Ping!";     // Setting the data to be sent.
    int len = strlen(data) + 1;            

    memcpy(pac + ICMP_HDRLEN, data, len);  // Copying the data to the packet right after the ICMP header.

    // Calculate the checksum and add it to the packet:

    header.icmp_cksum = calculate_checksum((unsigned short *)(pac), ICMP_HDRLEN + len);
    memcpy(pac, &header, ICMP_HDRLEN);

    // We return the number of bytes used in the packet.

    return ICMP_HDRLEN + len;
}
