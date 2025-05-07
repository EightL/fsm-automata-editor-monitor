/**
 * @file   udp_channel.hpp
 * @brief  Declares UdpChannel: a non-blocking UDP IChannel transport.
 *
 * UdpChannel binds a UDP socket to a local endpoint and sends/receives
 * JSON-based Packet structs to/from a specified peer address.
 *
 * @author Martin Ševčík (xsevcim00)
 * @author Jakub Lůčný (xlucnyj00)
 * @date   2025-05-06
 */

#ifndef IO_BRIDGE_UDP_CHANNEL_HPP
#define IO_BRIDGE_UDP_CHANNEL_HPP

#include "channel.hpp"
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

namespace io_bridge {

/**
 * @class UdpChannel
 * @brief Final implementation of IChannel using UDP datagrams.
 *
 * Creates a non-blocking UDP socket, binds it locally, and
 * sends/receives Packet JSON payloads to/from a fixed peer.
 */
class UdpChannel final : public IChannel {
public:
    /**
     * @brief Constructs the UDP channel.
     * @param bindAddr  Local address ("IP:port") to bind on.
     * @param peerAddr  Remote peer address ("IP:port") to send to.
     * @note noexcept: on failure socket remains invalid (m_sock < 0).
     */
    UdpChannel(const std::string &bindAddr, const std::string &peerAddr) noexcept;

    /**
     * @brief Closes the UDP socket if open.
     */
    ~UdpChannel() noexcept override;

    /**
     * @brief Sends a Packet as a single UDP datagram.
     * @param pkt  Packet containing JSON string.
     * @return     True if all bytes were sent; false otherwise.
     */
    bool send(const Packet &pkt) noexcept override;

    /**
     * @brief Non-blocking receive of a UDP datagram.
     * @param[out] pkt  Filled with the JSON string on success.
     * @return     True if data was received; false if no data or error.
     */
    bool poll(Packet &pkt) noexcept override;

private:
    int           m_sock{-1};               /**< UDP socket FD or -1 on error */
    sockaddr_in   m_peer{};                 /**< Cached peer address */
    static constexpr size_t BUF_SIZE = 2048;/**< Receive buffer capacity */
    char          m_buf[BUF_SIZE];          /**< Temporary recv buffer */
};

} // namespace io_bridge

#endif // IO_BRIDGE_UDP_CHANNEL_HPP
