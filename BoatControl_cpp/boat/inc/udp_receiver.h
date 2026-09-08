#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <stdint.h>
#include "detection_packet.h"

class UdpReceiver
{
public:
    UdpReceiver(uint16_t port, int timeout_ms);
    ~UdpReceiver(void);
    bool receive(bcc::DetectionPacket& out);
private:
    int sock_;
};

#endif