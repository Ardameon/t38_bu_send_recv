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
#include <pthread.h>
#include <sys/stat.h>

#include "fax.h"
#include "fax_bu.h"
#include "msg_proc.h"

#define  IP4_MAX_LEN 15
#define  PORT_NUM 44444

#define  SETUP_MSG "SETUP 000001 GG 192.168.23.20:44444 192.168.23.20:55555"
#define  FAX_SEND_PORT_START 44433
#define  FAX_RECV_PORT_START 44533
#define  FAX_MAX_SESSION_CNT 40


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

typedef enum {
    RECV,
    SEND
} ses_mode_e;

static pthread_t start_fax_session(ses_mode_e mode, const char *call_id,
                                   uint16_t local_port, uint32_t remote_ip,
                                   uint16_t remote_port, const char *filename,
                                   fax_session_t **f_session)
{
    pthread_t thread = -1;
    fax_session_t *fax_session;

    fax_session = malloc(sizeof(fax_session_t));

    fax_session->pvt.caller = mode;
    strcpy(fax_session->pvt.filename, filename);
    fax_session->local_port = local_port;

    if(remote_ip) fax_session->remote_ip = remote_ip;
    if(remote_port) fax_session->remote_port = remote_port;
    strcpy(fax_session->call_id, call_id);

    pthread_create(&thread, NULL, &fax_worker_thread, fax_session);

    *f_session = fax_session;

    return thread;
}

extern int sendRelese(fax_session_t *f_session)
{
    char buffer[512];
    int  buflen = 0;
    sig_message_t *msg_req;

    sig_msgCreateRelease(f_session->call_id, (sig_message_rel_t **)(&msg_req));

    sig_msgCompose(msg_req, buffer, sizeof(buffer));

    buflen = strlen(buffer);
    printf("buffer_len: %d buffer: '%.*s'", buflen, buflen, buffer);

    if ((sendPacket((uint8_t *)buffer, buflen)) != -1) {
        printf("Data sent to %s:%d: len=%d buf='%s'\n",
               inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port),
               buflen, buffer);
    } else {
        perror("sendPacket() failed");
    }

    return 0;
}

int main(int argc, char *argv[])
{

    int ret_status;
    int i, res;
    char local_ip[IP4_MAX_LEN], remote_ip[IP4_MAX_LEN];
    uint16_t local_port = PORT_NUM, remote_port;
    uint16_t send_port, recv_port, fax_rem_port;
    uint32_t fax_rem_ip;
    char buffer[512];
    char tiff_filename[64], recv_filename[64];
    int buflen;
    int len;
    int ses_count = 0;
    int sock;
    char call_id_str[FAX_MAX_SESSION_CNT];
    sig_message_t *msg_req = NULL;
    sig_message_t *msg_resp = NULL;
    fax_session_t *f_session[FAX_MAX_SESSION_CNT][2];
    pthread_t      f_thread[FAX_MAX_SESSION_CNT][2];
    struct stat st;

    if (argc < 5) {
        printf("Not enough arguments: [REMOTE_HOST] [REMOTE_PORT] [SESSION_COUNT] [TIFF_FILE_TO_SEND]\n");
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    ses_count = strtoul(argv[3], NULL, 10);

    if (ses_count > FAX_MAX_SESSION_CNT) {
        printf("Too many sessions! No more then %d supported\n", FAX_MAX_SESSION_CNT);
        ret_status = EXIT_FAILURE;
        goto _exit;
    }

    strncpy(tiff_filename, argv[4], sizeof(tiff_filename));
    if (stat(tiff_filename, &st)) {
        printf("Tiff file error ('%s'): %s\n", tiff_filename, strerror(errno));
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
        sprintf(call_id_str, "0000000%d", i + 1);

        send_port = FAX_SEND_PORT_START + i;
        recv_port = FAX_RECV_PORT_START + i;

        f_session[i][0] = f_session[i][1] = NULL;

        sig_msgCreateSetup(call_id_str,
                           ntohl(inet_addr(local_ip)), send_port,
                           ntohl(inet_addr(local_ip)), recv_port,
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

        fax_rem_ip = ((sig_message_ok_t *)msg_resp)->ip;
        fax_rem_port = ((sig_message_ok_t *)msg_resp)->port;

        /* Sending thread */
        f_thread[i][0] = start_fax_session(SEND, call_id_str, send_port,
                                           fax_rem_ip, fax_rem_port,
                                           tiff_filename,
                                           &(f_session[i][0]));

        /* If we do without fork() - received filed will be distorted,
         * don't know why.
         *
         * Most of all - spandsp bug, not sure
         */
        if(!fork())
        {
            if(msg_resp->type != FAX_MSG_OK) exit(1);


            sprintf(recv_filename, "received_fax_%02d.tif", i);

            /* Receiving thread */
            f_thread[i][1] = start_fax_session(RECV, call_id_str, recv_port, 0,
                                               0, recv_filename,
                                               &f_session[i][0]);

            pthread_join(f_thread[i][1], NULL);
            if(f_session[i][1]) free(f_session[i][1]);

            exit(0);
        }

        sig_msgDestroy(msg_req);
        sig_msgDestroy(msg_resp);

        msg_req = NULL;
        msg_resp = NULL;
    }

    for(i = 0; i < ses_count; i++)
    {
        pthread_join(f_thread[i][0], NULL);
    }

    for(i = 0; i < ses_count; i++)
    {
        if(f_session[i][0]) free(f_session[i][0]);
    }

_exit:
    if (sock > -1) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    return ret_status;
}
