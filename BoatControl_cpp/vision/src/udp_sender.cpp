#include <iostream>
#include <cstring>
#include "udp_sender.h"

UdpSender::UdpSender(const std::string& host, uint16_t port)
{
#ifdef _WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0){
        std::cout << "Winsock 초기화 실패\n";
        this->sock_ = kInvalidSocket;
        return;
    }
#endif
    this->sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(this->sock_ == kInvalidSocket) {
        std::cout << "Socket 생성 실패\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(port);
    if(inet_pton(AF_INET, host.c_str(), &this->deviceAddr.sin_addr) != 1) {
        std::cout << "잘못된 IP 형식: " << host << "\n";
#ifdef _WIN32
        closesocket(this->sock_);
        WSACleanup();
#else
        close(this->sock_);
#endif
        this->sock_ = kInvalidSocket;
        return;
    }
}

UdpSender::~UdpSender(void)
{
    if(this->sock_ != kInvalidSocket) {
#ifdef _WIN32
        closesocket(this->sock_);
        WSACleanup();
#else
        close(this->sock_);
#endif
    }
}

bool UdpSender::send(const bcc::DetectionPacket& packet)
{
    if(this->sock_ == kInvalidSocket) { return false; }

    auto sendToPack = sendto(
        this->sock_,
        reinterpret_cast<const char*>(&packet), sizeof(packet),
        0,
        reinterpret_cast<const sockaddr*>(&this->deviceAddr), sizeof(this->deviceAddr)
    );

    if(sendToPack != static_cast<int>(sizeof(packet))) {
#ifdef _WIN32
        std::cout << "전송 실패: " << WSAGetLastError() << "\n";
#else
        std::cout << "전송 실패: " << strerror(errno) << "\n";
#endif
        return false;
    }
    
    return true;
}