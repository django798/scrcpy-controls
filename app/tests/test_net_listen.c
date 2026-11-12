/* Copyright (C) 2026 moasm */
/* SPDX-License-Identifier: Apache-2.0 */
#include "common.h"

#include <assert.h>
#include <stdint.h>
#ifndef _WIN32
# include <arpa/inet.h>
# include <netinet/in.h>
# include <sys/socket.h>
#endif

#include "util/net.h"

static uint16_t
bound_port(sc_socket socket) {
#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    sc_raw_socket raw = socket->socket;
#else
    sc_raw_socket raw = socket;
#endif
    struct sockaddr_in address;
#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    assert(!getsockname(raw, (struct sockaddr *) &address, &size));
    return ntohs(address.sin_port);
}

static void
test_listeners_are_exclusive(void) {
    sc_socket first = net_socket();
    sc_socket conflicting = net_socket();
    sc_socket second = net_socket();
    assert(first != SC_SOCKET_NONE && conflicting != SC_SOCKET_NONE
           && second != SC_SOCKET_NONE);
    assert(net_listen(first, IPV4_LOCALHOST, 0, 1));
    uint16_t first_port = bound_port(first);
    assert(first_port);
    // Windows SO_REUSEADDR previously let this second listener bind too:
    // different devices' video/audio/control sockets could reach the same app.
    assert(!net_listen(conflicting, IPV4_LOCALHOST, first_port, 1));
    assert(net_close(conflicting));
    assert(net_listen(second, IPV4_LOCALHOST, 0, 1));
    uint16_t second_port = bound_port(second);
    assert(second_port && second_port != first_port);

    sc_socket client_a = net_socket();
    sc_socket client_b = net_socket();
    assert(client_a != SC_SOCKET_NONE && client_b != SC_SOCKET_NONE);
    assert(net_connect(client_a, IPV4_LOCALHOST, first_port));
    assert(net_connect(client_b, IPV4_LOCALHOST, second_port));
    sc_socket accepted_a = net_accept(first);
    sc_socket accepted_b = net_accept(second);
    assert(accepted_a != SC_SOCKET_NONE && accepted_b != SC_SOCKET_NONE);

    // Check both directions so isolation covers video and input traffic.
    char value;
    assert(net_send_all(client_a, "A", 1) == 1);
    assert(net_send_all(client_b, "B", 1) == 1);
    assert(net_recv_all(accepted_a, &value, 1) == 1 && value == 'A');
    assert(net_recv_all(accepted_b, &value, 1) == 1 && value == 'B');
    assert(net_send_all(accepted_a, "a", 1) == 1);
    assert(net_send_all(accepted_b, "b", 1) == 1);
    assert(net_recv_all(client_a, &value, 1) == 1 && value == 'a');
    assert(net_recv_all(client_b, &value, 1) == 1 && value == 'b');

    assert(net_close(accepted_a));
    assert(net_close(accepted_b));
    assert(net_close(client_a));
    assert(net_close(client_b));
    assert(net_close(first));
    assert(net_close(second));
}

int
main(void) {
    assert(net_init());
    test_listeners_are_exclusive();
    net_cleanup();
    return 0;
}
