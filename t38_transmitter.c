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

#define  IP4_MAX_LEN 15
#define  PORT_NUM 44444

int gl_sock;



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

int main(int argc, char *argv[])
{
    int sock = -1;
    int ret_status = 0;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int reuse = 1;
    int /*i,*/ res;
    char local_ip[IP4_MAX_LEN], remote_ip[IP4_MAX_LEN];
    unsigned local_port = PORT_NUM, remote_port;
    char buf[1024] = {0};
    int len;
    socklen_t rem_addr_len = sizeof(remote_addr);

    if (argc < 3) {
        printf("Not enough arguments: [REMOTE_HOST] [REMOTE_PORT]\n");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    strcpy(local_ip, getLocalIP("eth0"));

    printf("Local IP is '%s:%d'\n", local_ip, local_port);

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr(local_ip);
    local_addr.sin_port = htons(local_port);

    if ((res = getHostAddr(argv[1], argv[2], &remote_addr))) {
        printf("getaddrinfo() error: %s\n", gai_strerror(res));
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    strcpy(remote_ip, inet_ntoa(remote_addr.sin_addr));
    remote_port = ntohs(remote_addr.sin_port);

    printf("Remote IP is '%s:%d'\n", remote_ip, remote_port);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() failed");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

//    fcntl(sock, F_SETFL, O_NONBLOCK);

    signal(SIGINT, &sigint_handler);

    gl_sock = sock;

    printf("Client socket created. fd: %d\n", sock);

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt() failed");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind() failed");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    char buffer[256];
    sprintf(buffer, "EROR 12345678987654321 INTERNAL_ERR\r\n");
    int buflen = strlen(buffer);

    printf("buffer_len: %d buffer: '%.*s'", buflen, buflen, buffer);

    if ((len = sendto(sock, buffer, buflen, 0, (struct sockaddr *)&remote_addr,
                      sizeof(remote_addr)) != -1)) {
        printf("Data sent to %s:%d: len=%d buf='%s'\n", remote_ip, remote_port,
               buflen, buffer);
    } else {
        perror("sendto() failed");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    if ((len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&remote_addr,
                        (socklen_t *)&rem_addr_len)) != -1) {
        printf("Data received form %s:%d len=%d buf='%s'\n",
               inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port),
               len, buf);
    } else {
        perror("recvfrom() failed");
        ret_status = EXIT_FAILURE;
    }

_exit:
    if (sock > -1) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    return ret_status;
}
