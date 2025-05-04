
// udp_channel.hpp
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

class UdpChannel final : public IChannel {
public:
    // bindAddr and peerAddr in "IP:port" form, e.g. "0.0.0.0:45454"
    UdpChannel(const std::string &bindAddr, const std::string &peerAddr) noexcept;
    ~UdpChannel() noexcept override;

    bool send(const Packet &pkt) noexcept override;
    bool poll(Packet &pkt) noexcept override;

private:
    int m_sock{-1};
    sockaddr_in m_peer{};
    static constexpr size_t BUF_SIZE = 2048;
    char m_buf[BUF_SIZE];
};

} // namespace io_bridge

#endif // IO_BRIDGE_UDP_CHANNEL_HPP