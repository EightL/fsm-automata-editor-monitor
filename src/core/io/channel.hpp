#ifndef IO_BRIDGE_CHANNEL_HPP
#define IO_BRIDGE_CHANNEL_HPP

#include <string>
#include <memory>

namespace io_bridge {

// A self-contained JSON packet
struct Packet {
    std::string json;
};

// Abstract transport interface: send and poll packets
class IChannel {
public:
    virtual ~IChannel() = default;

    // Send a packet; returns false only on catastrophic failure
    virtual bool send(const Packet &pkt) noexcept = 0;

    // Non-blocking poll for a packet; returns true if pkt is filled
    virtual bool poll(Packet &pkt) noexcept = 0;
};

using ChannelPtr = std::shared_ptr<IChannel>;

} // namespace io_bridge

#endif // IO_BRIDGE_CHANNEL_HPP
