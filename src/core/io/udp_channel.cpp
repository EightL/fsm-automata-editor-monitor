// udp_channel.cpp
#include "udp_channel.hpp"
#include <cstring>

namespace io_bridge {

static bool parseEndpoint(const std::string &str, sockaddr_in &out) noexcept {
    auto pos = str.find(':');
    if (pos == std::string::npos) return false;
    std::string ip = str.substr(0, pos);
    uint16_t port = static_cast<uint16_t>(std::stoi(str.substr(pos+1)));
    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &out.sin_addr) != 1) return false;
    return true;
}

UdpChannel::UdpChannel(const std::string &bindAddr, const std::string &peerAddr) noexcept {
    // create socket
    m_sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock < 0) return;
    {
        int opt = 1;
        ::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    // non-blocking
    fcntl(m_sock, F_SETFL, O_NONBLOCK);
    // bind
    sockaddr_in local{};
    if (!parseEndpoint(bindAddr, local)) return;
    if (::bind(m_sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) return;
    // peer
    if (!parseEndpoint(peerAddr, m_peer)) return;
}

UdpChannel::~UdpChannel() noexcept {
    if (m_sock >= 0) ::close(m_sock);
}

bool UdpChannel::send(const Packet &pkt) noexcept {
    if (m_sock < 0) return false;
    auto data = pkt.json.data();
    auto len  = static_cast<int>(pkt.json.size());
    int res = ::sendto(m_sock,
                       data, len, 0,
                       reinterpret_cast<sockaddr*>(&m_peer), sizeof(m_peer));
    return res == len;
}

bool UdpChannel::poll(Packet &pkt) noexcept {
    if (m_sock < 0) return false;
    sockaddr_in src{};
    socklen_t slen = sizeof(src);
    int n = ::recvfrom(m_sock,
                       m_buf, BUF_SIZE, 0,
                       reinterpret_cast<sockaddr*>(&src), &slen);
    if (n <= 0) return false; // no data or error
    pkt.json.assign(m_buf, static_cast<size_t>(n));
    return true;
}

} // namespace io_bridge
