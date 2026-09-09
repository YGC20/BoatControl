#include "udp_receiver.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

UdpReceiver::UdpReceiver(uint16_t port, int timeout_ms)
{
    this->sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(this->sock_ < 0) {
        std::cout << "Socket 생성 실패\n";
        return;
    }

    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(this->sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = INADDR_ANY;
    addr.sin_port           = htons(port);
    if(bind(this->sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cout << "Bind 실패\n";
        close(this->sock_);
        this->sock_ = -1;
        return;
    } 

}

UdpReceiver::~UdpReceiver(void)
{
    if(this->sock_ >= 0) { close(this->sock_); }
}

bool UdpReceiver::receive(bcc::DetectionPacket& out)
{
    if(sock_ < 0) {
        std::cout << "Socket 연결 실패\n";
        return false;
    }

    bcc::DetectionPacket packet{};
    ssize_t n = recvfrom(sock_, &packet, sizeof(packet), 0, nullptr, nullptr);
    
    if(n != static_cast<ssize_t>(sizeof(packet))) { return false; }
    if(packet.magic != bcc::kDetectionPacketMagic) { return false; }

    out = packet;
    return true;
}