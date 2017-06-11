#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE                         8192

int main(int argc, char *argv[])
{
        int sock;
        struct sockaddr_in addr;

        char request[BUFSIZE];
        int request_len;

        char response[BUFSIZE];

        int nbytes_total = 0;
        int nbytes_recv;

        char *p = NULL;

        // check argument
        if (argc != 2)
        {
                fprintf(stderr, "Usage: %s [server IP address]\n", argv[0]);
                return -1;
        }

        // create socket
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0)
        {
                fprintf(stderr, "Could not create socket.\n");
                return -1;
        }

        // make address info
        memset(&addr, 0, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        addr.sin_port = htons(80);

        if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        {
                fprintf(stderr, "Could not connect to %s:80\n", argv[1]);
                close(sock);
        }

        // make request
        snprintf(request, sizeof(request),
                "GET / HTTP/1.1\r\n"
                "User-Agent: Wget/1.17.1 (linux-gnu)\r\n"
                "Accept: */*\r\n"
                "Accept-Encoding: identity\r\n"
                "Host: %s\r\n"
                "Connection: Keep-Alive\r\n\r\n",
                argv[1]);

        request_len = strlen(request);
        while (nbytes_total < request_len)
        {
                int nbytes_sent = send(sock, request + nbytes_total, request_len - nbytes_total, 0);
                if (nbytes_sent < 0)
                {
                        fprintf(stderr, "write() function has failed.\n");
                        close(sock);

                        return -1;
                }

                fprintf(stderr, "Sent '%d' bytes\n", nbytes_sent);
                nbytes_total += nbytes_sent;
        }

        fprintf(stderr, "sent request '%s'\n", request);

        // get response
        nbytes_total = 0;
        while ((nbytes_recv = recv(sock, response, BUFSIZE, 0)) > 0)
        {
                fprintf(stderr, "received %d bytes(%s)\n", nbytes_recv, response);

                if (!p)
                        p = (char *) malloc(nbytes_recv + 1);
                else
                        p = (char *) realloc(p, nbytes_total + nbytes_recv + 1);

                memcpy(p + nbytes_total, response, nbytes_recv);
                nbytes_total += nbytes_recv;

                sleep(1);
        }

        fprintf(stderr, "total bytes %d\n", nbytes_total);
        fprintf(stderr, "response buffer is %s\n", p);

        if (p)
                free(p);

        // close socket
        close(sock);

        return 0;
}
