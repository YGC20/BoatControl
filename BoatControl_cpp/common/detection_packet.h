/**
 * 통신 포멧
 */
#pragma once
#include <cstdint>

namespace bcc {

constexpr std::uint16_t kDetectionPacketMagic = 0xD5C1;
constexpr int kMaxDetections = 8;
constexpr std::uint16_t kDefaultBoatPort = 5555;

#pragma pack(push, 1)

struct DetectionEntry {
    std::int32_t class_id;
    float        score;
    float        x1, y1, x2, y2;
};

struct DetectionPacket {
    std::uint16_t magic = kDetectionPacketMagic;
    std::uint16_t frame_width;
    std::uint16_t frame_height;
    std::uint16_t num_detections;
    std::uint32_t frame_id;
    DetectionEntry detections[kMaxDetections];
};

#pragma pack(pop)

} // namespace bcc
