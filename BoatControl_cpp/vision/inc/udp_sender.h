#pragma once

#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include <string>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <WS2tcpip.h>

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#endif

#include "detection_packet.h"

class UdpSender
{
public:
    UdpSender(const std::string& host, uint16_t port);
    ~UdpSender();
    bool send(const bcc::DetectionPacket& packet);
private:
#ifdef _WIN32
    using socket_t = SOCKET;
    static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
    using socket_t = int;
    static constexpr socket_t kInvalidSocket = -1;
#endif
    socket_t sock_;
    sockaddr_in deviceAddr;

};

#endif // UDP_SENDER_H