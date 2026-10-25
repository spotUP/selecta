/* Regression (I266): sel_net_send reported a cancelled wait as SEL_NET_E_TIMEOUT,
 * so a TLS handshake on a cancelled job thread retried at full CPU until its
 * deadline and library close hung (dui-viz hit its 120 s timeout). A cancelled
 * wait must be SEL_NET_E_IO, like sel_net_recv. */
#include "net/net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

int main(void) {
    struct sockaddr_in a = {0};
    socklen_t al = sizeof a;
    static const volatile unsigned gen = 1;
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int h;
    int r;
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (ls < 0 || bind(ls, (struct sockaddr*)&a, sizeof a) != 0 || listen(ls, 1) != 0 ||
        getsockname(ls, (struct sockaddr*)&a, &al) != 0) {
        printf("FAIL listener\n");
        return 1;
    }
    h = sel_net_open("127.0.0.1", ntohs(a.sin_port), 2000);
    CHECK(h >= 0);
    if (h >= 0) {
        /* generation moved since the fetch started: the thread is cancelled */
        sel_net_set_cancel(&gen, 0);
        r = sel_net_send(h, "x", 1, 2000);
        CHECK(r == SEL_NET_E_IO);
        sel_net_set_cancel(NULL, 0);
        /* not cancelled: the same send succeeds */
        r = sel_net_send(h, "x", 1, 2000);
        CHECK(r == 0);
        sel_net_close(h);
    }
    close(ls);
    if (g_fail == 0) {
        printf("net_cancel_send_ok\n");
    }
    return g_fail != 0;
}
