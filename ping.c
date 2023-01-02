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

#define IP4_HDRLEN 20    // IPv4 header len without options
#define ICMP_HDRLEN 8    // ICMP header len for echo messages

// # Function Headers #

unsigned short calculate_checksum(unsigned short *paddress, int len);
int makePacket(int seq, char *pac);

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

    int rawsock = -1;;
    if ((rawsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d\n", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        return -1;
    }

    // We now intialize the variables below:

    int seq = 0;                   // We set a sequence counter to 0, which will be used to identify the ICMP packets sent by this program.
    char pac[IP_MAXPACKET];        // We also create a buffer which will contain the ICMP packet.
    float time = 0;                // Lastly, we create a variable which will contain the time it took to get a reply from the destination.

    // We now begin a loop of sending ICMP ping packets to the destination, and receiving replies from it.

    printf("Pinging the address: %s\n", ip);

    while(1)
    {
        // We used the sleep() method to wait 1 second between each ping, just for making the printout more readable.
        
        sleep(1);
        
        // We first create the ICMP packet, and get its length.

        int len = makePacket(seq, pac);

        // For each packet sent, we track the time of sending it and getting a reply from the destination.
        // We use the gettimeofday() function to get the time in seconds and microseconds.

        struct timeval start, end;
        gettimeofday(&start, 0);

        // We use the sendto() function to send the packet to the destination.

        int send = sendto(rawsock, pac, len, 0, (struct sockaddr *)&address, sizeof(address));
        if (send == -1)
        {
            printf("Sending packet failed with error: %d\n", errno);
            return -1;
        }

        // We now clear the packet buffer receiving a reply from the destination.

        bzero(pac, IP_MAXPACKET);

        socklen_t addlen = sizeof(address);
        
        // We now begin a receiving loop, which will only break when we get a reply from the destination.
        // The continuation rule is: As long as we can receive packets, we will continue to do so.

        ssize_t recv = -1;
        while ( recv = recvfrom(rawsock, pac, sizeof(pac), 0, (struct sockaddr *)&address, &addlen) )   // If we got a reply, we break the loop. 
        {
            if (recv > 0)
            {   
                break;     // If we got a reply, we break the loop.
            }

            
        }

        // We now get the end time of receiving the reply from the destination, and calculate the time it took to get the reply.

        gettimeofday(&end, 0);

        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
        time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
    
        // We will now print data about the reply we got from the destination.

        printf("-- Reply from %s : seq = %d, bytes = %ld, time = %.3f ms.\n", ip, seq, recv, time);

        // We now increment the sequence counter, and clear the packet buffer for the next iteration of the loop, if any occur.

        seq++;
        bzero(pac, IP_MAXPACKET);
    }

    // If we broke the loop, it means we didn't receive a reply propperly.
    // Therefore, we close the socket and exit the program.

    printf("Closing socket, goodbye!.\n");

    close(rawsock);

    return 0;
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

int makePacket(int seq, char *pac)
{
    // Creating the parts of the ICMP header and adding it to the packet:
    
    struct icmp header;

    header.icmp_type = ICMP_ECHO;         // Message Type (Consists of 8 bits)      - the type of the message, which in our case is an echo message.
    header.icmp_code = 0;                 // Message Code (Consists of 8 bits)      - 0 represents the package is an echo request.
    header.icmp_id = 18;                  // Message ID (Consists of 16 bits)       - helps the receiver to identify the full message created by the packets.
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