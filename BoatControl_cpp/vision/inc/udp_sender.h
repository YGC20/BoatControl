#pragma once

#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include <string>
#ifndef NOMINMAX
#define NOMINMAX // windows.h의 min/max 매크로가 std::min/std::max와 충돌하는 것을 방지
#endif
#include <winsock2.h>
#include <WS2tcpip.h>
#include "detection_packet.h"

class UdpSender
{
public:
    UdpSender(const std::string& host, uint16_t port);
    ~UdpSender();
    bool send(const dsu::DetectionPacket& packet);
private:
    SOCKET sock_;
    SOCKADDR_IN deviceAddr;
};

#endif // UDP_SENDER_H