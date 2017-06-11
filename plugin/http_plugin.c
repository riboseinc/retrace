#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "../common.h"
#include "../malloc.h"
#include "../write.h"
#include "../read.h"
#include "../file.h"
#include "../sock.h"

#include "../plugin.h"

struct http_session
{
        int sockfd;                             // this socket description is registered in connect call

        unsigned char *send_buffer;
        int send_buffer_len;

        int send_matched;

        unsigned char *recv_buffer;
        int recv_buffer_len;
        int recv_offset;

        char recv_redirect_fpath[256];

        struct http_session *next;
        struct http_session *prev;
};

// HTTP plugin data
typedef struct rtr_http_sock_ctx
{
        struct http_session *sessions;
        int sessions_count;

        pthread_mutex_t mutex;
} rtr_http_sock_ctx;

static rtr_http_sock_ctx g_http_ctx;

// add redirect session
static int add_http_session(int sockfd)
{
        real_malloc = RETRACE_GET_REAL(malloc);

        // allocate new session
        struct http_session *sess = (struct http_session *) real_malloc(sizeof(struct http_session));
        if (!sess)
                return -1;

        // init session
        memset(sess, 0, sizeof(struct http_session));

        // set socket descriptor
        sess->sockfd = sockfd;

        // lock mutex
        pthread_mutex_lock(&g_http_ctx.mutex);

        // add session into context
        if (g_http_ctx.sessions_count == 0)
        {
                g_http_ctx.sessions = sess;
        }
        else
        {
                struct http_session *p = g_http_ctx.sessions;

                while (p->next)
                        p = p->next;

                p->next = sess;
                sess->prev = p;
        }

        g_http_ctx.sessions_count++;

        // unlock mutex
        pthread_mutex_unlock(&g_http_ctx.mutex);

        return 0;
}

// free http session
static void free_http_session(struct http_session *p)
{
        real_free = RETRACE_GET_REAL(free);

        // free buffers
        if (p->send_buffer)
                real_free(p->send_buffer);

        if (p->recv_buffer)
                real_free(p->recv_buffer);

        // free self
        real_free(p);

        return;
}

// remove http session
static void remove_http_session(struct http_session *p)
{
        // lock mutex
        pthread_mutex_lock(&g_http_ctx.mutex);

        // set list
        if (g_http_ctx.sessions = p)
                g_http_ctx.sessions = NULL;
        else
        {
                if (p->prev)
                        p->prev->next = p->next;
                if (p->next)
                        p->next->prev = p->prev;
        }

        // free http session
        free_http_session(p);

        g_http_ctx.sessions_count--;

        // unlock mutex
        pthread_mutex_unlock(&g_http_ctx.mutex);

        return;
}

// get session by socket
static struct http_session *get_http_session_by_sock(int sockfd)
{
        // lock mutex
        pthread_mutex_lock(&g_http_ctx.mutex);

        // get session object by socket
        struct http_session *p = g_http_ctx.sessions;
        while (p)
        {
                if (p->sockfd == sockfd)
                {
                        pthread_mutex_unlock(&g_http_ctx.mutex);
                        return p;
                }

                p = p->next;
        }

        // unlock mutex
        pthread_mutex_unlock(&g_http_ctx.mutex);

        return NULL;
}

// add buffer into http session
static void add_http_session_data(struct http_session *sess, const void *buf, int len, int is_send, int *final)
{
        if (is_send)
        {
                // allocate buffer and copy origin buffer
                if (sess->send_buffer_len == 0)
                        sess->send_buffer = (unsigned char *) malloc(len);
                else
                        sess->send_buffer = (unsigned char *) realloc(sess->send_buffer, sess->send_buffer_len + len);

                memcpy(sess->send_buffer + sess->send_buffer_len, buf, len);
                sess->send_buffer_len += len;

                // check request is ended
                if (strstr((const char *) buf, "\r\n\r\n"))
                        *final = 1;
        }

        return;
}

// http plugin connect function
static int http_plugin_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
        rtr_config config = NULL;

        char *match_ip = NULL, *redirect_ip = NULL;
        int match_port, redirect_port;

        real_connect = RETRACE_GET_REAL(connect);
        real_free = RETRACE_GET_REAL(free);

        // check socket family
        if (addr->sa_family != AF_INET)
                return real_connect(sockfd, addr, addrlen);

        // set IP address and port number
        const char *dst_ipaddr = inet_ntoa(((struct sockaddr_in *) addr)->sin_addr);
        int dst_port = ntohs(((struct sockaddr_in *) addr)->sin_port);

        trace_printf(1, "http_plugin: connect(%d, %s:%d, %d)\n", sockfd, dst_ipaddr, dst_port, addrlen);
        while (rtr_get_config_multiple(&config, "connect",
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_INT,
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_INT,
                        ARGUMENT_TYPE_END,
                        &match_ip,
                        &match_port,
                        &redirect_ip,
                        &redirect_port))
        {
                // check IP address and port number is matched
                if (strcmp(match_ip, dst_ipaddr) == 0 && match_port == dst_port)
                {
                        struct sockaddr_in redirect_addr;

                        // add session into context
                        if (add_http_session(sockfd) != 0)
                        {
                                trace_printf(1, "http_plugin: add_http_session(): Out of memory!\n");

                                real_free(match_ip);
                                real_free(redirect_ip);

                                break;
                        }

                        // set redirect address
                        memset(&redirect_addr, 0, sizeof(redirect_addr));

                        redirect_addr.sin_family = AF_INET;
                        redirect_addr.sin_addr.s_addr = inet_addr(redirect_ip);
                        redirect_addr.sin_port = htons(redirect_port);

                        trace_printf(1, "http_plugin: redirect connect(%d, %s:%d, %d)\n", sockfd, redirect_ip, redirect_port,
                                                        sizeof(struct sockaddr_in));

                        // update file descriptor
                        file_descriptor_update(sockfd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, redirect_ip, redirect_port);

                        // free buffers
                        real_free(match_ip);
                        real_free(redirect_ip);

                        rtr_confing_close(config);

                        return real_connect(sockfd, (struct sockaddr *) &redirect_addr, sizeof(redirect_addr));
                }

                // free buffers
                if (match_ip)
                        real_free(match_ip);

                if (redirect_ip)
                        real_free(redirect_ip);

                match_ip = redirect_ip = NULL;
        }

        // close config
        if (config)
        {
                rtr_confing_close(config);
                config = NULL;
        }

        return real_connect(sockfd, addr, addrlen);
}

// http plugin send function
static ssize_t http_plugin_send(int sockfd, const void *buf, size_t len, int flags)
{
        int send_final = 0;

        real_send = RETRACE_GET_REAL(send);
        real_free = RETRACE_GET_REAL(free);

        // check socket has registered
        struct http_session *p = get_http_session_by_sock(sockfd);
        if (!p)
                return real_send(sockfd, buf, len, flags);

        // set send buffer
        add_http_session_data(p, buf, len, 1, &send_final);
        if (!send_final)
                return len;

        // send final flag is set
        rtr_config config = NULL;
        char *send_match = NULL, *recv_fpath = NULL;

        // check config if send buffer is in matching set
        while (rtr_get_config_multiple(&config, "send",
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_STRING,
                        ARGUMENT_TYPE_END,
                        &send_match,
                        &recv_fpath))
        {
                // compare send buffer
                if (strncmp(p->send_buffer, send_match, strlen(send_match)) == 0)
                {
                        p->send_matched = 1;

                        strcpy(p->recv_redirect_fpath, recv_fpath);
                        p->recv_redirect_fpath[strlen(recv_fpath)] = '\0';

                        trace_printf(1, "http_plugin: Found sending redirection rule %s=>%s\n", send_match, recv_fpath);
                }

                if (send_match)
                        real_free(send_match);

                if (recv_fpath)
                        real_free(recv_fpath);

                send_match = recv_fpath = NULL;

                if (p->send_matched)
                        break;
        }

        // close config
        if (config)
        {
                rtr_confing_close(config);
        }

        // if send buffer is not in matching set, then send buffer
        if (!p->send_matched)
        {
                int total_bytes = 0;
                while (total_bytes < p->send_buffer_len)
                {
                        int nbytes = real_send(sockfd, p->send_buffer + total_bytes, p->send_buffer_len - total_bytes, flags);
                        if (nbytes < 0)
                                return -1;

                        total_bytes += nbytes;
                }
        }

        // free send buffer
        real_free(p->send_buffer);

        p->send_buffer = NULL;
        p->send_buffer_len = 0;

        return len;
}

// HTTP plugin recv function
static const char resp_200_format[] =
                "HTTP/1.1 200 OK\r\n" \
                "Accept-Ranges: bytes\r\n" \
                "Content-Length: %lu\r\n" \
                "Vary: Accept-Encoding\r\n" \
                "Connection: Keep-Alive" \
                "Content-Type: text/html\r\n\r\n" \
                "%s";


static const char resp_404_format[] =
                "HTTP/1.1 404 Not Found\r\n" \
                "Content-Length: %lu\r\n" \
                "Connection: close\r\n" \
                "Content-Type: text/html\r\n\r\n" \
                "%s";

#define HTTP_MAX_HEADER_LEN             512
#define HTTP_404_RESP_MSG               "Not Found"

static ssize_t http_plugin_recv(int sockfd, void *buf, size_t len, int flags)
{
        struct stat st;

        real_recv = RETRACE_GET_REAL(recv);
        // real_stat = RETRACE_GET_REAL(stat);
        real_malloc = RETRACE_GET_REAL(malloc);
        real_free = RETRACE_GET_REAL(free);

        // get session by socket
        struct http_session *p = get_http_session_by_sock(sockfd);
        if (!p)
                return real_send(sockfd, buf, len, flags);

        // check if sending condition is matched
        if (!p->send_matched)
                return real_send(sockfd, buf, len, flags);

        trace_printf(1, "http_plugin: matched condition, sending contents of file '%s'\n", p->recv_redirect_fpath);

        // when recv is called at fist, then it should read contents from file
        if (p->recv_offset == 0)
        {
                char *response = NULL;

                // get size of file
                if (stat(p->recv_redirect_fpath, &st) == 0 && st.st_size > 0)
                {
                        FILE *fp;

                        real_fopen = RETRACE_GET_REAL(fopen);
                        real_fread = RETRACE_GET_REAL(fread);
                        real_fclose = RETRACE_GET_REAL(fclose);

                        // open file
                        fp = real_fopen(p->recv_redirect_fpath, "r");
                        if (fp)
                        {
                                char *resp_data;

                                // allocate buffer
                                resp_data = (char *) real_malloc(st.st_size + 1);
                                if (resp_data)
                                {
                                        // read buffer from file
                                        real_fread(resp_data, 1, st.st_size, fp);
                                        resp_data[st.st_size] = '\0';
                                        
                                        real_fclose(fp);

                                        // make response
                                        response = (char *) real_malloc(st.st_size + HTTP_MAX_HEADER_LEN);
                                        if (!response)
                                        {
                                                real_free(resp_data);
                                                return -1;
                                        }

                                        memset(response, 0, st.st_size + HTTP_MAX_HEADER_LEN);
                                        snprintf(response, st.st_size + HTTP_MAX_HEADER_LEN, resp_200_format, st.st_size, resp_data);
                                }
                        }
                }

                // if getting response has failed, then send 404
                if (!response)
                {
                        response = (char *) real_malloc(HTTP_MAX_HEADER_LEN);
                        if (!response)
                                return -1;

                        memset(response, 0, HTTP_MAX_HEADER_LEN);
                        snprintf(response, HTTP_MAX_HEADER_LEN, resp_404_format, strlen(HTTP_404_RESP_MSG), HTTP_404_RESP_MSG);
                }

                // set recv buffer for redirecting
                p->recv_buffer = (unsigned char *) response;
                p->recv_buffer_len = strlen(response);

                trace_printf(1, "http_plugin: Total redirect recv buffer len is '%d' bytes\n", p->recv_buffer_len);
        }

        // check offset if response has been sent completely
        if (p->recv_offset == p->recv_buffer_len)
        {
                // free buffers and flag
                real_free(p->recv_buffer);
                p->recv_buffer = NULL;

                p->recv_buffer_len = p->recv_offset = 0;
                p->send_matched = 0;

                return 0;
        }

        // set response content
        if ((p->recv_offset + len) > p->recv_buffer_len)
        {
                trace_printf(1, "http_plugin: Sending redirect recv buffer '%d' bytes.\n", len);
                len = p->recv_buffer_len - p->recv_offset;
        }

        memcpy(buf, p->recv_buffer + p->recv_offset, len);
        p->recv_offset += len;

        return len;
}

// HTTP plugin write function
static ssize_t http_plugin_write(int sockfd, const void *buf, size_t len)
{
        real_write = RETRACE_GET_REAL(write);
        return real_write(sockfd, buf, len);
}

// HTTP plugin read function
static ssize_t http_plugin_read(int sockfd, void *buf, size_t len)
{
        real_read = RETRACE_GET_REAL(read);
        return real_read(sockfd, buf, len);
}

// HTTP plugin close function
static int http_plugin_close(int sockfd)
{
        real_close = RETRACE_GET_REAL(close);

        // get session by socket
        struct http_session *p = get_http_session_by_sock(sockfd);
        if (!p)
                return real_close(sockfd);

        trace_printf(1, "http_plugin: Close socket (%d)\n", sockfd);

        // remove session
        remove_http_session(p);

        return real_close(sockfd);
}

// initialize http plugin context
static int http_plugin_ctx_init()
{
        // initialize context object
        memset(&g_http_ctx, 0, sizeof(g_http_ctx));

        // initiilize mutex
        pthread_mutex_init(&g_http_ctx.mutex, NULL);

        return 0;
}

// HTTP plugin structure
rtr_plugin_sock_t rtr_http_data =
{
        .p_connect = http_plugin_connect,
        .p_send = http_plugin_send,
        .p_recv = http_plugin_recv,
        .p_write = http_plugin_write,
        .p_read= http_plugin_read,
        .p_close = http_plugin_close,
};

// register plugin
int rtr_plugin_register(void **plugin_data)
{
        int ret;

        // initialize context
        ret = http_plugin_ctx_init();
        if (ret != 0)
                return -1;

        // set plugin data
        *plugin_data = (void *) &rtr_http_data;

        return 0;
}
