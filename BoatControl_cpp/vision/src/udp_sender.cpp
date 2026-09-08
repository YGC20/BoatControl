#include <iostream>
#include "udp_sender.h"

UdpSender::UdpSender(const std::string& host, uint16_t port)
{
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0){
        std::cout << "Winsock 초기화 실패\n";
        this->sock_ = INVALID_SOCKET;
        return;
    }

    this->sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(this->sock_ == INVALID_SOCKET) {
        std::cout << "Socket 생성 실패\n";
        WSACleanup();
        return;
    }

    deviceAddr.sin_family = AF_INET;
    deviceAddr.sin_port = htons(port);
    if(InetPtonA(AF_INET, host.c_str(), &this->deviceAddr.sin_addr) != 1) {
        std::cout << "잘못된 IP 형식: " << host << "\n";
        closesocket(this->sock_);
        this->sock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }
}

UdpSender::~UdpSender(void)
{
    if(this->sock_ != INVALID_SOCKET) {
        closesocket(this->sock_);
        WSACleanup();
    }
}

bool UdpSender::send(const bcc::DetectionPacket& packet)
{
    if(this->sock_ == INVALID_SOCKET) { return false; }

    int sendToPack = sendto(
        this->sock_,
        reinterpret_cast<const char*>(&packet), sizeof(packet),
        0,
        reinterpret_cast<const SOCKADDR*>(&this->deviceAddr), sizeof(this->deviceAddr)
    );

    if(sendToPack != static_cast<int>(sizeof(packet))) {
        std::cout << "전송 실패: " << WSAGetLastError() << "\n";
        return false;
    }
    
    return true;
}