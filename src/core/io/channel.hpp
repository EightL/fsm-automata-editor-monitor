/**
 * @file   channel.hpp
 * @brief  Defines the abstract IChannel interface and Packet struct
 *         for JSON-based transport channels.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 *
 */

#ifndef IO_BRIDGE_CHANNEL_HPP
#define IO_BRIDGE_CHANNEL_HPP

#include <string>
#include <memory>

namespace io_bridge {

/**
 * @struct Packet
 * @brief A self-contained JSON packet for transport.
 *
 * The Packet struct holds a single JSON message as a std::string.
 */
struct Packet {
    std::string json;
};

/**
 * @class IChannel
 * @brief Abstract transport interface for sending and polling Packets.
 *
 * IChannel defines a common API for packet-based transports.  Concrete
 * implementations must provide send() and poll() methods.
 */
class IChannel {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IChannel() = default;

    /**
     * @brief Send a packet.
     *
     * @param pkt  The Packet to send.
     * @return     True on success; false on catastrophic failure.
     */
    virtual bool send(const Packet &pkt) noexcept = 0;

    /**
     * @brief Non-blocking poll for an incoming packet.
     *
     * @param[out] pkt  The Packet to fill if data is available.
     * @return          True if a packet was received and pkt is populated.
     */
    virtual bool poll(Packet &pkt) noexcept = 0;
};

/**
 * @typedef ChannelPtr
 * @brief Shared pointer alias for IChannel.
 */
using ChannelPtr = std::shared_ptr<IChannel>;

} // namespace io_bridge

#endif // IO_BRIDGE_CHANNEL_HPP
