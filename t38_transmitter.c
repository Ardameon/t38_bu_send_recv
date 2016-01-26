#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <stdint.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdint.h>

//#include "fax.h"
#include "fax_bu.h"
#include "msg_proc.h"

#define  IP4_MAX_LEN 15
#define  PORT_NUM 44444

#define  SETUP_MSG "SETUP 000001 GG 192.168.23.20:44444 192.168.23.20:55555"
#define  FAX_SEND_PORT_START 44433
#define  FAX_RECV_PORT_START 44533
#define  FAX_MAX_SESSION_CNT 20


int gl_sock;

struct sockaddr_in remote_addr;
socklen_t rem_addr_len = sizeof(remote_addr);

void sigint_handler(int sig) {
    printf("SIGINT handler: close socket.\n");
    shutdown(gl_sock, SHUT_RDWR);
    close(gl_sock);
}

static const char *getLocalIP(const char *iface)
{
    static char ip_addr_str[IP4_MAX_LEN + 1];
    struct ifreq ifr;

    strcpy(ifr.ifr_name, iface);

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(s, SIOCGIFADDR, &ifr);
    close(s);

    struct sockaddr_in *sa = (struct sockaddr_in *)&ifr.ifr_addr;

    strcpy(ip_addr_str, inet_ntoa(sa->sin_addr));

    return ip_addr_str;
}

static int getHostAddr(char *ip, char *port, struct sockaddr_in *sa)
{
    int n;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_DGRAM;

    n = getaddrinfo(ip, port, &hints, &res);
    if (n == 0) {
        memcpy(sa, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
    }

    return n;
}

static int sendPacket(const uint8_t *buf, int size)
{
    return sendto(gl_sock, buf, size, 0, (struct sockaddr *)&remote_addr,
                      sizeof(remote_addr));
}

static int recvPacket(uint8_t *buf, int size)
{
    return recvfrom(gl_sock, buf, size, 0, (struct sockaddr *)&remote_addr,
                        (socklen_t *)&rem_addr_len);
}

static int socket_init(char *ip, uint16_t port)
{
    struct sockaddr_in local_addr;
    int sock = -1;
    int ret_val = 0;
    int reuse = 1;

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(ip);
    local_addr.sin_port = htons(port);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() failed");
        ret_val = -1;
        goto _exit;
    }

    signal(SIGINT, &sigint_handler);

    gl_sock = sock;

    printf("Client socket created. fd: %d\n", sock);

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt() failed");
        ret_val = -2;
        goto _exit;
    }

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind() failed");
        ret_val = -3;
        goto _exit;
    }

    ret_val = sock;

_exit:
    return ret_val;
}

int main(int argc, char *argv[])
{

    int ret_status;
    int i, res;
    char local_ip[IP4_MAX_LEN], remote_ip[IP4_MAX_LEN];
    unsigned local_port = PORT_NUM, remote_port;
    char buffer[512];
    int buflen;
    int len;
    int ses_count = 0;
    int sock;
    char call_id_str[20];
    sig_message_t *msg_req = NULL;
    sig_message_t *msg_resp = NULL;

    if (argc < 4) {
        printf("Not enough arguments: [REMOTE_HOST] [REMOTE_PORT] [SESSION_COUNT]\n");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    ses_count = strtoul(argv[3], NULL, 10);

    if (ses_count > FAX_MAX_SESSION_CNT) {
        printf("Too many sessions! No more then %d supported\n", FAX_MAX_SESSION_CNT);
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    strcpy(local_ip, getLocalIP("eth0"));

    printf("Local IP is '%s:%d'\n", local_ip, local_port);

    if ((sock = socket_init(local_ip, local_port)) < 0) {
        ret_status = EXIT_FAILURE;
        goto _exit;
    }


    if ((res = getHostAddr(argv[1], argv[2], &remote_addr))) {
        printf("getaddrinfo() error: %s\n", gai_strerror(res));
        ret_status = EXIT_FAILURE;
        goto _exit;
    }


    strcpy(remote_ip, inet_ntoa(remote_addr.sin_addr));
    remote_port = ntohs(remote_addr.sin_port);

    printf("Remote IP is '%s:%d'\n", remote_ip, remote_port);

    for(i = 0; i < ses_count; i++)
    {
        sprintf(call_id_str, "0000000%d", i);

        sig_msgCreateSetup(call_id_str,
                           ntohl(inet_addr(local_ip)), FAX_SEND_PORT_START + i,
                           ntohl(inet_addr(local_ip)), FAX_RECV_PORT_START + i,
                           FAX_MODE_GW_GW, (sig_message_setup_t **)(&msg_req));

        sig_msgCompose(msg_req, buffer, sizeof(buffer));

        buflen = strlen(buffer);
        printf("buffer_len: %d buffer: '%.*s'", buflen, buflen, buffer);

        if ((len = sendPacket((uint8_t *)buffer, buflen)) != -1) {
            printf("Data sent to %s:%d: len=%d buf='%s'\n",
                   inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port),
                   buflen, buffer);
        } else {
            perror("sendPacket() failed");
            ret_status = EXIT_FAILURE;
            goto _exit;
        }

        if ((len = recvPacket((uint8_t *)buffer, sizeof(buffer))) != -1) {
            printf("Data received form %s:%d len=%d buf='%s'\n",
                   inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port),
                   len, buffer);
        } else {
            perror("recvfrom() failed");
            ret_status = EXIT_FAILURE;
        }

        sig_msgParse(buffer, &msg_resp);
        sig_msgPrint(msg_resp, buffer, sizeof(buffer));

        printf("MSG Response: %s\n", buffer);

        sig_msgDestroy(msg_req);
        sig_msgDestroy(msg_resp);

        sleep(1);
    }

//    fcntl(sock, F_SETFL, O_NONBLOCK);

_exit:
    if (sock > -1) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    return ret_status;
}
