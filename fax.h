#ifndef FAX_H_
#define FAX_H_

#include <sys/socket.h>
#include <stdint.h>

#include "spandsp.h"
#include "spandsp/expose.h"
#include "udptl.h"

typedef enum {
    FAX_TRANSPORT_T38_MOD,
    FAX_TRANSPORT_AUDIO_MOD
} fax_transport_mod_e;

typedef struct fax_params_s {
    struct {
        t38_terminal_state_t *t38_state;
        t38_core_state_t     *t38_core;
        udptl_state_t        *udptl_state;

        char filename[64];
        char *ident;
        char *header;

        uint8_t use_ecm:     1,
                disable_v17: 1,
                verbose:     1,
                caller:      1,
                done:        1,
                reserve:     3;
    } pvt;

    struct {
        uint16_t T38FaxVersion;
        uint32_t T38MaxBitRate;
        uint32_t T38FaxMaxBuffer;
        uint32_t T38FaxMaxDatagram;
        char    *T38FaxRateManagement;
        char    *T38FaxUdpEC;
        char    *T38VendorInfo;
        uint8_t T38FaxFillBitRemoval:  1,
                T38FaxTranscodingMMR:  1,
                T38FaxTranscodingJBIG: 1,
                reserve:               5;
    } t38_options;

    fax_transport_mod_e transport_mode;

    int socket_fd;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t remote_ip;
    struct sockaddr_in *rem_addr;

    uint8_t fax_success;

} fax_session_t;

typedef struct ses_param_t {
    uint8_t  caller;

    fax_session_t fax_session;
} ses_param_t;

void fax_params_init(fax_session_t *f_session);
void fax_params_destroy(fax_session_t *f_session);
void fax_params_set_default(fax_session_t *f_session);

void *fax_worker_thread(void *data);



#endif /* FAX_H_ */
