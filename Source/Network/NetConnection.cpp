#include "NetIncludes.h"
#include "NetConnection.h"

#include <future>

#include "crc.h"
#include "P2P_interface.h"
#include "NetConnectionAux.h"

#ifdef EMSCRIPTEN
#include "emscripten.h"
constexpr uint32_t TRANSPORT_RECV_SLEEP = 0;
#else
constexpr uint32_t TRANSPORT_RECV_SLEEP = 10;
#endif


///////// NetAddress //////////////

NetAddress::NetAddress() = default;

#ifndef EMSCRIPTEN

const static char* NET_ADDRESS_UDP_PING_DATA = "HostPing";
const static char* NET_ADDRESS_UDP_PONG_DATA = "HostPong";

#ifdef PERIMETER_SDL3
NET_Server* NetAddress::listenTCP() const {
    NET_Server* tcp_server;
    if (host.empty() || host == "0.0.0.0") {
        tcp_server = NET_CreateServer(nullptr, port, 0);
    } else {
        tcp_server = NET_CreateServer(net_addr, port, 0);
    }
    if (tcp_server == nullptr) {
        fprintf(stderr, "NetAddress::listenTCP failed to open TCP listener address %s error %s\n", getString().c_str(), SDL_GetError());
        return nullptr;
    }
    return tcp_server;
}

NET_StreamSocket* NetAddress::connectTCP(int32_t timeout) const {
#else
TCPsocket NetAddress::connectTCP(int32_t timeout) const {
    if (addr4 == INADDR_NONE) {
        fprintf(stderr, "NetAddress::connectTCP ip4 address is NONE\n");
        return nullptr;
    }
    IPaddress ipaddr;
    ipaddr.host = addr4;
    SDLNet_Write16(port, &ipaddr.port);
#endif

    //Ping over UDP with reduced timeout in case host is down so that TCP open won't stay waiting all day
    //This is used mostly for connections against relay
    if (0 < timeout) {
#ifdef PERIMETER_SDL3
        NET_DatagramSocket* udp_socket = NET_CreateDatagramSocket(nullptr, 0, 0);
        if (udp_socket == nullptr) {
            fprintf(stderr, "NetAddress::connectTCP failed to open UDP address %s error %s\n", getString().c_str(), SDL_GetError());
            return nullptr;
        }
#else
        UDPsocket udp_socket = SDLNet_UDP_Open(0);
        if (udp_socket == nullptr) {
            fprintf(stderr, "NetAddress::connectTCP failed to open UDP address %s error %s\n", getString().c_str(), SDLNet_GetError());
            return nullptr;
        }
#endif

        size_t ping_str_len = strlen(NET_ADDRESS_UDP_PING_DATA);
        size_t pong_str_len = strlen(NET_ADDRESS_UDP_PONG_DATA);
#ifndef PERIMETER_SDL3
        const static size_t UPD_PACKET_DATA_LEN = 32;
        uint8_t upd_packet_data[UPD_PACKET_DATA_LEN] = {};
#endif

        int32_t start_time = clocki();
        const int time_sleep = 10;
        while (true) {
            if (start_time + timeout < clocki()) {
                fprintf(stderr, "NetAddress::connectTCP timeout waiting for UDP ping response address %s\n",
                        getString().c_str());
                return nullptr;
            }
#ifdef PERIMETER_SDL3
            if (!NET_SendDatagram(udp_socket, net_addr, port, NET_ADDRESS_UDP_PING_DATA, static_cast<int>(ping_str_len))) {
                fprintf(stderr, "NetAddress::connectTCP failed to send UDP ping address %s error %s\n",
                        getString().c_str(), SDL_GetError());
                return nullptr;
            }

            Sleep(time_sleep);

            NET_Datagram* udp_packet = nullptr;
            bool udp_recv_ok = NET_ReceiveDatagram(udp_socket, &udp_packet);
            if (!udp_recv_ok) {
                if (udp_packet) {
                    NET_DestroyDatagram(udp_packet);
                }

                //Error ocurred, abort
                fprintf(stderr, "NetAddress::connectTCP failed to recv UDP datagram address %s error %s\n",
                        getString().c_str(), SDL_GetError());
                return nullptr;
            } else if (udp_packet) {
                if (udp_packet->buflen != pong_str_len) {
                    fprintf(stderr, "NetAddress::connectTCP failed UDP pong len %" PRIi32 " address %s error %s\n",
                            udp_packet->buflen, getString().c_str(), SDL_GetError());
                    return nullptr;
                }
                if (memcmp(udp_packet->buf, NET_ADDRESS_UDP_PONG_DATA, pong_str_len) != 0) {
                    fprintf(stderr, "NetAddress::connectTCP failed UDP pong data mismatch address %s error %s\n",
                            getString().c_str(), SDL_GetError());
                    return nullptr;
                }

                NET_DestroyDatagram(udp_packet);
            } else {
                //No packet yet, wait until timeout
                continue;
            }

#else
            memcpy(reinterpret_cast<char*>(upd_packet_data), NET_ADDRESS_UDP_PING_DATA, ping_str_len);

            UDPpacket udp_packet;
            udp_packet.channel = -1;
            udp_packet.data = upd_packet_data;
            udp_packet.len = static_cast<int>(ping_str_len);
            udp_packet.maxlen = UPD_PACKET_DATA_LEN - 1;
            udp_packet.status = 0;
            udp_packet.address = ipaddr;
            int status = SDLNet_UDP_Send(udp_socket, -1, &udp_packet);
            if (status != 1) {
                fprintf(stderr, "NetAddress::connectTCP failed to send UDP ping %d address %s error %s\n",
                        status, getString().c_str(), SDLNet_GetError());
                return nullptr;
            }

            Sleep(time_sleep);

            status = SDLNet_UDP_Recv(udp_socket, &udp_packet);
            if (status == 1) {
                if (udp_packet.len != pong_str_len) {
                    fprintf(stderr, "NetAddress::connectTCP failed UDP pong len %" PRIi32 " address %s error %s\n",
                            udp_packet.len, getString().c_str(), SDLNet_GetError());
                    return nullptr;
                }
                if (memcmp(udp_packet.data, NET_ADDRESS_UDP_PONG_DATA, pong_str_len) != 0) {
                    fprintf(stderr, "NetAddress::connectTCP failed UDP pong data mismatch address %s error %s\n",
                            getString().c_str(), SDLNet_GetError());
                    return nullptr;
                }
                break;
            } else if (status != 0) {
                //Keep trying until timeout
                fprintf(stderr, "NetAddress::connectTCP failed to recv UDP ping %d address %s error %s\n",
                        status, getString().c_str(), SDLNet_GetError());
                continue;
            }
#endif
        }
    }

    //Do TCP connection
#ifdef PERIMETER_SDL3
    NET_StreamSocket* tcp_socket = NET_CreateClient(net_addr, port, 0);
    if (tcp_socket == nullptr) {
        fprintf(stderr, "NetAddress::connectTCP failed to open TCP address %s error %s\n", getString().c_str(), SDL_GetError());
        return nullptr;
    }
    NET_Status status = NET_WaitUntilConnected(tcp_socket, timeout);
    if (status != NET_SUCCESS) {
        NET_DestroyStreamSocket(tcp_socket);
        fprintf(stderr, "NetAddress::connectTCP got non success opening TCP address %s: status %d error %s\n", getString().c_str(), status, SDL_GetError());
        return nullptr;
    }
#else
    TCPsocket tcp_socket = SDLNet_TCP_Open(&ipaddr);
    if (tcp_socket == nullptr) {
        fprintf(stderr, "NetAddress::connectTCP failed to open TCP address %s error %s\n", getString().c_str(), SDLNet_GetError());
    }
#endif

    return tcp_socket;
}
#endif //EMSCRIPTEM

NetAddress::~NetAddress() {
#ifdef PERIMETER_SDL3
    if (net_addr) {
        NET_UnrefAddress(net_addr);
    }
#endif
}

bool NetAddress::resolve(NetAddress& address, const std::string& host, uint16_t default_port) {
    std::string host_tmp;
    uint16_t port;
    if (default_port == 0) {
        default_port = PERIMETER_IP_PORT_DEFAULT;
    }
    size_t pos = host.find(':');
    if (pos == std::string::npos) {
        host_tmp = host;
        port = default_port;
    } else {
        host_tmp = host.substr(0, pos);
        std::string port_str = host.substr(pos + 1);
        char* end;
        port = static_cast<uint16_t>(strtol(port_str.c_str(), &end, 10));
        if (!port) port = default_port;
    }

#ifdef EMSCRIPTEN
    address.host = host_tmp;
    address.port = port;
#else //EMSCRIPTEN
#ifdef PERIMETER_SDL3
    NET_Address* addr = NET_ResolveHostname(host.c_str());
    if (addr) {
        fprintf(stderr, "Error preparing to resolve host %s: %s\n", host.c_str(), SDL_GetError());
        return false;
    }
    int timeout = 4;
    NET_Status status = NET_WaitUntilResolved(addr, timeout);
    if (status != NET_SUCCESS) {
        fprintf(stderr, "Error resolving host %s: status %d error %s\n", host.c_str(), status, SDL_GetError());
        return false;
    }
    address.host = host_tmp;
    address.net_addr = addr;
    address.port = port;
#else //PERIMETER_SDL3
    IPaddress ipaddr;
    int32_t ret = SDLNet_ResolveHost(&ipaddr, host_tmp.c_str(), port) == 0;
    if (ret < 0 || ipaddr.host == INADDR_NONE) {
        fprintf(stderr, "Error resolving host %s: %s\n", host.c_str(), SDLNet_GetError());
        return false;
    }
    address.host = host_tmp;
    address.addr4 = ipaddr.host;
    address.port = SDLNet_Read16(&ipaddr.port);
#endif //PERIMETER_SDL3
#endif //EMSCRIPTEN
    return true;
}

uint16_t NetAddress::get_port() const {
    return port;
}

NetAddress& NetAddress::operator=(const NetAddress& other) {
    this->host = other.host;
    this->port = other.port;
#ifndef EMSCRIPTEN
#ifdef PERIMETER_SDL3
    NET_RefAddress(net_addr);
    this->net_addr = other.net_addr;
#else
    this->addr4 = other.addr4;
#endif
#endif
    return *this;
}

bool NetAddress::operator==(const NetAddress& other) const {
    return this->host == other.host
        && this->port == other.port;
}

void NetAddress::reset() {
    host = "";
    port = 0;
#ifndef EMSCRIPTEN
#ifdef PERIMETER_SDL3
    net_addr = nullptr;
#else
    addr4 = INADDR_NONE;
#endif
#endif
}

std::string NetAddress::getAddress() const {
    std::string address;

    if (!host.empty()) {
        address += host;

        if (port) {
            address += ":" + std::to_string(port);
        }
    } else {
        address = "none";
    }

    return address;
}

std::string NetAddress::getString() const {
    std::string address = getAddress();

#ifndef EMSCRIPTEN
    if (!address.empty()) {
#ifdef PERIMETER_SDL3
        const char* addrstr = nullptr;
        if (net_addr) {
            addrstr = NET_GetAddressString(net_addr);
        }
        if (addrstr) {
            address += " (";
            address += addrstr;
            address += ")";
        }
#else
        if (addr4 != INADDR_NONE) {
            address += " (";
            for (size_t i = 0; i < 4; ++i) {
                if (i > 0) address += ".";
                uint8_t v = (addr4 >> (8 * i)) & 0xff;
                address += std::to_string(v);
            }
            address += ")";
        }
#endif
    }
#endif

    return address;
}

///////// NetTransport //////////////

NetTransport* NetTransport::listen(const NetAddress& address) {
#ifdef PERIMETER_SDL3
    NET_Server* socket = address.listenTCP();

    if (!socket) {
        return nullptr;
    }

    return new NetTransportTCP(socket);
#else
    //In SDL2 the pathway is the same
    return new NetTransport::connect(address, 0);
#endif
}


NetTransport* NetTransport::connect(const NetAddress& address, int32_t timeout) {
#ifdef EMSCRIPTEN
    int32_t handle = EM_ASM_INT((
        return Module.transportCreate($0);
    ), address.getAddress().c_str());

    if (handle == -1) {
        return nullptr;
    }

    return new NetTransportWS(handle);
#else // EMSCRIPTEN
#ifdef PERIMETER_SDL3
    NET_StreamSocket* socket = address.connectTCP(timeout);
#else
    TCPsocket socket = address.connectTCP(timeout);
#endif

    if (!socket) {
        return nullptr;
    }
    
    return new NetTransportTCP(socket);
#endif
}

int32_t NetTransport::send(const void* buffer, uint32_t len, int32_t timeout) {
    if (is_closed()) {
        return NT_STATUS_CLOSED;
    }
    if (buffer == nullptr) {
        ErrH.Abort("NetTransport::send got null buffer");
    }
    
    int32_t sent = 0;
    int32_t start_time = clocki();
    while (sent < len) {
        if (0 < timeout && start_time + timeout < clocki()) {
            return NT_STATUS_TIMEOUT;
        }
        int32_t amount = send_raw(static_cast<const uint8_t*>(buffer) + sent, static_cast<int32_t>(len) - sent, timeout);
        if (amount < 0) {
            if (amount != NT_STATUS_TIMEOUT) {
                fprintf(stderr, "NetTransport::send data failed result %d sent %d len %d\n", amount, sent, len);
            }
            return amount;
        }
        sent += amount;
    }

    return sent;
}

int32_t NetTransport::receive(void* buffer, uint32_t minlen, uint32_t maxlen, int32_t timeout) {
    if (is_closed()) {
        return NT_STATUS_CLOSED;
    }
    if (buffer == nullptr) {
        ErrH.Abort("NetTransport::receive got null buffer");
    }

    int32_t received = 0;
    int32_t start_time = clocki();
    bool has_timeout = 0 < timeout;
    while (received < maxlen
    && (0 == minlen || received < minlen)) {
        if (has_timeout && start_time + timeout < clocki()) {
            return NT_STATUS_TIMEOUT;
        }
        int32_t amount = receive_raw(static_cast<uint8_t*>(buffer) + received, static_cast<int32_t>(maxlen) - received, timeout);
        if (has_timeout && amount == NT_STATUS_NO_DATA) {
            //Keep waiting
#ifdef EMSCRIPTEN
            emscripten_sleep(TRANSPORT_RECV_SLEEP);
#else
            Sleep(TRANSPORT_RECV_SLEEP);
#endif
            continue;
        }
        if (amount < 0) {
            if (amount != NT_STATUS_TIMEOUT && amount != NT_STATUS_NO_DATA) {
                fprintf(stderr, "NetTransport::receive failed amount %" PRIi32
                                " minlen %" PRIu32 " maxlen %" PRIu32 "\n", amount, minlen, maxlen);
                close();
            }
            return amount;
        }
        received += amount;
    }
    if (received < minlen || maxlen < received) {
        fprintf(stderr, "NetTransport::receive length mismatch received %" PRIi32
                " minlen %" PRIu32 " maxlen %" PRIu32 "\n", received, minlen, maxlen);
        close();
        return NT_STATUS_ERROR;
    }

    return received;
}

///////// NetTransportTCP //////////////

#ifdef PERIMETER_SDL3
NetTransportTCP::NetTransportTCP(NET_Server* server_) {
    server = server_;
}

NetTransportTCP::NetTransportTCP(NET_StreamSocket* socket_) {
    socket = socket_;
}
#else
NetTransportTCP::NetTransportTCP(TCPsocket socket_) {
    socket = socket_;
    socket_set = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(socket_set, socket);
}
#endif

void NetTransportTCP::close() {
#ifdef PERIMETER_SDL3
    if (server) {
        NET_DestroyServer(server);
        server = nullptr;
    }
    if (socket) {
        NET_DestroyStreamSocket(socket);
        socket = nullptr;
    }
#else
    if (socket_set) {
        if (socket) {
            SDLNet_TCP_DelSocket(socket_set, socket);
        }
        SDLNet_FreeSocketSet(socket_set);
        socket_set = nullptr;
    }
    if (socket) {
        SDLNet_TCP_Close(socket);
        socket = nullptr;
    }
#endif
}

int32_t NetTransportTCP::send_raw(const uint8_t* buffer, uint32_t len, int32_t timeout) {
    if (socket == nullptr) {
        return NT_STATUS_CLOSED;
    }
#ifdef PERIMETER_SDL3
    if (!NET_WriteToStreamSocket(socket, buffer, static_cast<int32_t>(len))) {
        fprintf(stderr, "NetTransportTCP::send data failed len %" PRIu32 " %s\n", len, SDL_GetError());
        return NT_STATUS_ERROR;
    }
    //Should return pending = 0 unless timeout occurs, but the caller will handle incomplete transfers/timeout anyway
    int pending = NET_WaitUntilStreamSocketDrained(socket, timeout);
    if (pending < 0) {
        fprintf(stderr, "NetTransportTCP::send got pending %d len %" PRIu32 "error %s\n", pending, len, SDL_GetError());
        return NT_STATUS_ERROR;
    }
    int32_t amount = static_cast<int>(len) - pending;
#else //PERIMETER_SDL3
    //May return 0 if closed
    int32_t amount = SDLNet_TCP_Send(socket, buffer, static_cast<int32_t>(len));
    if (amount == 0) {
        return NT_STATUS_CLOSED;
    } else if (amount <= 0) {
        fprintf(stderr, "NetTransportTCP::send data failed result %" PRIi32 " len %" PRIu32 " %s\n", amount, len, SDLNet_GetError());
        return NT_STATUS_ERROR;:
    }
#endif
    return amount;
}

int32_t NetTransportTCP::receive_raw(uint8_t* buffer, uint32_t len, int32_t _timeout) {
    if (socket == nullptr) {
        return NT_STATUS_CLOSED;
    }
#ifdef PERIMETER_SDL3
    int32_t amount = NET_ReadFromStreamSocket(socket, buffer, static_cast<int32_t>(len));
    if (amount < 0) {
        fprintf(stderr, "NetTransportTCP::receive failed amount %" PRIi32 " len %" PRIu32 " %s\n", amount, len, SDL_GetError());
        return NT_STATUS_ERROR;
    } else if (amount == 0) {
        return NT_STATUS_NO_DATA;
    }
#else // PERIMETER_SDL3
#ifdef GPX
    if (len == 8 && _timeout == 0) {
#endif

    int32_t n = SDLNet_CheckSockets(socket_set, 0);
    if (n == -1) {
        fprintf(stderr, "CheckSockets error: %s\n", SDLNet_GetError());
        // most of the time this is a system error, where perror might help you.
        perror("SDLNet_CheckSockets");
    } else if (n == 0) {
        return NT_STATUS_NO_DATA;
    }

#ifndef EMSCRIPTEN
    if (SDLNet_SocketReady(socket) == 0) {
        return NT_STATUS_NO_DATA;
    }
#endif

#ifdef GPX
    }
#endif

    //May return 0 if closed
    int32_t amount = SDLNet_TCP_Recv(socket, buffer, static_cast<int32_t>(len));
    if (amount == 0) {
        return NT_STATUS_CLOSED;
    } else if (amount <= 0) {
        fprintf(stderr, "NetTransportTCP::receive failed amount %" PRIi32 " len %" PRIu32 " %s\n", amount, len, SDLNet_GetError());
        return NT_STATUS_ERROR;
    }
#endif // PERIMETER_SDL3
    return amount;
}

#ifdef PERIMETER_SDL3
bool NetTransportTCP::acceptIncoming(NetTransport** transport) {
    NET_StreamSocket* incoming_socket = nullptr;
    if (!NET_AcceptClient(server, &incoming_socket)) {
        fprintf(stderr, "NetTransportTCP::acceptIncoming accept client error %s\n", SDL_GetError());
        return false;
    }
    if (incoming_socket) {
        *transport = new NetTransportTCP(incoming_socket);
    }
    return true;
}
#else //PERIMETER_SDL3
bool NetTransportTCP::acceptIncoming(NetTransport** transport) {
    if (!socket) {
        return false;
    }
    TCPsocket incoming_socket = SDLNet_TCP_Accept(socket);
    if (!incoming_socket) {
        SDLNet_SetError(nullptr);
    }
    *transport = new NetTransportTCP(incoming_socket);
    return true;
}
#endif //PERIMETER_SDL3

#ifdef EMSCRIPTEN
///////// NetTransportWS //////////////

NetTransportWS::NetTransportWS(int32_t handle): handle(handle) {}

int32_t NetTransportWS::send_raw(const uint8_t* buffer, uint32_t len, int32_t timeout) {
    return EM_ASM_INT((
        return Module.transportSend($0, $1, $2, $3);
    ), handle, buffer, len, timeout);
}

int32_t NetTransportWS::receive_raw(uint8_t* buffer, uint32_t len, int32_t timeout) {
    return EM_ASM_INT((
        return Module.transportReceive($0, $1, $2, $3);
    ), handle, buffer, len, timeout);
}

void NetTransportWS::close() {
    EM_ASM((
        Module.transportClose($0);
    ), handle);
    handle = -1;
}

bool NetTransportWS::is_closed() const {
    return handle == -1;
}

bool NetTransportWS::acceptIncoming(NetTransport** transport) {
    //Emscripten has no means to listen ports, thus no incoming connections
    return false;
}

#endif //EMSCRIPTEN

///////// NetConnection //////////////

const uint64_t NC_HEADER_MAGIC = 0xDE000000000000CA;
const uint64_t NC_HEADER_MASK  = 0xFF000000000000FF;

NetConnection::NetConnection(NetTransport* _transport, NETID _netid) {
    set_transport(_transport, _netid);
}

NetConnection::~NetConnection() {
    close();
}

void NetConnection::set_transport(NetTransport* _transport, NETID _netid) {
    //Call close to remove any existing transport and reset connectionstate
    close();
    
    //Set new transport
    netid = _netid;
    transport = _transport;
    if (hasTransport()) {
        state = NC_STATE_HAS_TRANSPORT;
    }
}

void NetConnection::close(bool error) {
    switch (state) {
        case NC_STATE_HAS_TRANSPORT:
            state = error ? NC_STATE_ERROR : NC_STATE_CLOSED;
            break;
        case NC_STATE_HAS_CLIENT:
            state = error ? NC_STATE_ERROR_PENDING : NC_STATE_CLOSE_PENDING;
            break;
        case NC_STATE_ERROR:
            if (!error) {
                state = NC_STATE_CLOSED;
            }
            break;
        case NC_STATE_ERROR_PENDING:
        case NC_STATE_CLOSE_PENDING:
        case NC_STATE_CLOSED:
            break;
    }
    if (transport) {
        transport->close();
        delete transport;
        transport = nullptr;
    }
}

int32_t NetConnection::send(const XBuffer* data, NETID source, NETID destination, int32_t timeout) {
    if (!hasTransport()) {
        return -1;
    }
    if (data == nullptr) {
        fprintf(stderr, "NetConnection::send NETID 0x%" PRIX64 " null buffer\n", netid);
        ErrH.Abort("Got null buffer in send");
    }
    if (data->tell() == 0) {
        xassert(0);
        fprintf(stderr, "NetConnection::send NETID 0x%" PRIX64 " data to sent is empty!\n", netid);
        return -2;
    }
    if (destination == NETID_NONE) {
        destination = this->netid;
    }
    xassert(destination != NETID_NONE);
    //Pinky promise that sending_buffer won't modify the data ptr
    XBuffer sending_buffer(const_cast<char*>(data->address()), data->tell());
    sending_buffer.set(data->tell());
    uint16_t flags = 0;
    
    //Compression, first thing to do to calculate actual length
    if (sending_buffer.tell() > PERIMETER_MESSAGE_COMPRESSION_SIZE) {
        XBuffer compress_buffer(sending_buffer.tell(), true);
        if (sending_buffer.compress(compress_buffer) == 0 
        && sending_buffer.tell() > compress_buffer.tell()) {
            sending_buffer = compress_buffer;
            flags |= PERIMETER_MESSAGE_FLAG_COMPRESSED;
        }
    }
    
    //Header size, not accounted in length that goes inside
    uint32_t header_len = sizeof(NC_HEADER_MAGIC);
    
    //Body size, the length of message +
    //Source + destination NETID info 
    uint32_t body_len = sending_buffer.tell() + sizeof(NETID) * 2;

    //Calculate message size
    int32_t msg_size = static_cast<int32_t>(body_len + header_len);
    if (msg_size > PERIMETER_MESSAGE_MAX_SIZE) {
        xassert(0);
        fprintf(stderr, "NetConnection::send NETID 0x%" PRIX64 " data too big len %d\n", netid, msg_size);
        return -2;
    }

    //Assemble header and data
    uint64_t header = NC_HEADER_MAGIC;
    header |= (static_cast<uint64_t>(flags & 0xFFFF) << 8);
    header |= (static_cast<uint64_t>(body_len & 0xFFFFFFFF) << 24);
    XBuffer xbuf(msg_size);
    //NOTE: Use write<> with explicit type to avoid type ambiguity from SDL_Swap64BE in some archs
#ifdef PERIMETER_SDL3
    xbuf.write<uint64_t>(SDL_Swap64BE(header));
    xbuf.write<uint64_t>(SDL_Swap64BE(source));
    xbuf.write<uint64_t>(SDL_Swap64BE(destination));
#else
    xbuf.write<uint64_t>(SDL_SwapBE64(header));
    xbuf.write<uint64_t>(SDL_SwapBE64(source));
    xbuf.write<uint64_t>(SDL_SwapBE64(destination));
#endif
    xbuf.write(sending_buffer, sending_buffer.tell());

#ifdef PERIMETER_DEBUG
    if (xbuf.tell() != msg_size) {
        fprintf(
            stderr,
            "NetConnection::send NETID 0x%" PRIX64 " written buffer mismatch"
            " buf %" PRIsize " msg %" PRIi32 " len %" PRIsize " %s\n",
            netid, xbuf.tell(), msg_size, sending_buffer.tell(),
#ifdef PERIMETER_SDL3
            SDL_GetError()
#else
            SDLNet_GetError()
#endif
        );
        close_error();
        return -4;
    }
#endif
    int32_t sent = transport->send(xbuf.buf, xbuf.tell(), timeout);

    if (sent != msg_size) {
        fprintf(
            stderr,
            "NetConnection::send NETID 0x%" PRIX64 " length mismatch"
            " sent %" PRIi32 " msg %" PRIi32 " len %" PRIsize " %s\n",
            netid, sent, msg_size, sending_buffer.tell(),
#ifdef PERIMETER_SDL3
            SDL_GetError()
#else
            SDLNet_GetError()
#endif
        );
        close_error();
        return -4;
    }
    
    return sent;
}

int32_t NetConnection::receive(NetConnectionMessage** packet_ptr, int32_t timeout) {
    if (!hasTransport()) {
        return -1;
    }

    //Get header first
    uint64_t header;
    int32_t amount = transport->receive(&header, 0, sizeof(header), timeout);
    if (amount == NetTransport::NT_STATUS_NO_DATA) {
        return 0;
    }
    if (amount < 0) {
        close_error();
        return amount;
    }
    if (amount != sizeof(header)) {
        fprintf(
            stderr, "NetConnection::receive NETID "
            "0x%" PRIX64 " header failed amount %d %s\n",
            netid, amount,
#ifdef PERIMETER_SDL3
            SDL_GetError()
#else
            SDLNet_GetError()
#endif
        );
        return -2;
    }
#ifdef PERIMETER_SDL3
    header = SDL_Swap64BE(header);
#else
    header = SDL_SwapBE64(header);
#endif
    
    //Check magic
    if ((header & NC_HEADER_MASK) != NC_HEADER_MAGIC) {
        fprintf(
            stderr, "NetConnection::receive NETID "
            "0x%" PRIX64 " header failed magic mismatch 0x%" PRIX64 " %s\n",
            netid, header,
#ifdef PERIMETER_SDL3
            SDL_GetError()
#else
            SDLNet_GetError()
#endif
        );
        return -2;
    }

    //Extract header stuff
    uint16_t flags = (header >> 8) & 0xFFFF;
    amount = static_cast<int32_t>((header >> 24) & 0xFFFFFFFF);

    //Ensure is not too big
    if (amount >= PERIMETER_MESSAGE_MAX_SIZE) {
        xassert(0);
        fprintf(stderr, "NetConnection::receive NETID 0x%" PRIX64 " header failed too long 0x%" PRIX64 " len %" PRIu32 "\n", netid, header, amount);
        return -2;
    }
    
    //Read data until all is received
    NetConnectionMessage* packet = *packet_ptr;
    if (packet == nullptr) {
        //Create new packet
        packet = new NetConnectionMessage(amount, this->netid, NETID_NONE);
    } else {
        //(re)allocate it to fit our data
        packet->alloc(amount);
    }
    int32_t received = transport->receive(
            packet->address() + packet->tell(), amount, amount, 
            std::max(timeout, 0) + RECV_DATA_AFTER_HEADER_TIMEOUT
    );
    if (received <= 0) {
        fprintf(stderr, "NetConnection::receive NETID"
                " 0x%" PRIX64 " data chunk failed amount %d received %d %s\n",
                netid, amount, received,
#ifdef PERIMETER_SDL3
                SDL_GetError()
#else
                SDLNet_GetError()
#endif
        );
        amount = -5;
    } else if (amount != received) {
        fprintf(stderr, "NetConnection::receive NETID"
                " 0x%" PRIX64 " data failed amount %d received %d %s\n",
                netid, amount, received,
#ifdef PERIMETER_SDL3
                SDL_GetError()
#else
                SDLNet_GetError()
#endif
        );
        amount = -6;
    }

    //Extract source and destination netids that is prepended before actual message data
    //If amount goes negative or 0 is fine because it would mark this packet as invalid 
    amount -= sizeof(NETID) * 2;
    if (amount < 0) {
        xassert(0);
        fprintf(stderr, "NetConnection::receive NETID 0x%" PRIX64 " header without message 0x%" PRIX64 " len %" PRIu32 "\n", netid, header, amount);
        amount = -7;
    } else if (amount == 0) {
        fprintf(stderr, "NetConnection::receive NETID 0x%" PRIX64 " message is empty 0x%" PRIX64 "\n", netid, header);
    } else {
        *packet > packet->source;
        *packet > packet->destination;
#ifdef PERIMETER_SDL3
        packet->source = SDL_Swap64BE(packet->source);
        packet->destination = SDL_Swap64BE(packet->destination);
#else
        packet->source = SDL_SwapBE64(packet->source);
        packet->destination = SDL_SwapBE64(packet->destination);
#endif
    }

    //Decompression
    if (0 < amount && flags & PERIMETER_MESSAGE_FLAG_COMPRESSED) {
        XBuffer output(amount, true);
        int32_t ret = packet->uncompress(output);
        if (ret != 0) {
            amount = -9;
        } else {
            amount = static_cast<int32_t>(output.tell());
            std::swap(*static_cast<XBuffer*>(packet), output);
        }
    } else {
        //Move message content that is after source/destination etc to start
        memmove(packet->address(), packet->address() + packet->tell(), amount);
        packet->set(amount);
    }
    
    //Set packet ptr or delete if we created the packet in this function
    if (*packet_ptr == nullptr) {
        if (0 < amount) {
            *packet_ptr = packet;
        } else {
            delete packet;
            close_error();
        }
    }
    return amount;
}
