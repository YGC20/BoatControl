#pragma once

#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <WS2tcpip.h>
#include "detection_packet.h"

class UdpSender
{
public:
    UdpSender(const std::string& host, uint16_t port);
    ~UdpSender();
    bool send(const bcc::DetectionPacket& packet);
private:
    SOCKET sock_;
    SOCKADDR_IN deviceAddr;
};

#endif // UDP_SENDER_H