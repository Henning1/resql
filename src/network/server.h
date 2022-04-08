#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "handler.h"
#include "def.h"

namespace network {


    class Server { 
    public:

        Server ( std::uint16_t port, Handler& handler ) : _port(port), _handler(handler) {
            this->_buffer.fill('\0');
        }

        ~Server() = default;
    
        void stop() {   
            _is_running = false;
        }

        void send ( std::uint32_t  client_id, 
                    std::string&&  message) {

            const auto length = std::uint64_t ( message.size() );
            auto response = std::string ( length + sizeof(length), '\0' );

            // Write header
            std::memcpy ( response.data(), 
                          static_cast<const void *>(&length), 
                          sizeof(length) );

            // Write data
            std::memmove ( response.data() + sizeof(length),
                           message.data(), 
                           length );

            ::send ( this->_client_sockets[client_id],
                     response.c_str(),
                     response.length(),
                     0);
        }

        bool listen() {
            this->_socket = socket ( AF_INET, SOCK_STREAM, 0 );
            if (this->_socket == 0) {
                return false;
            }

            char opt = 1;
            if ( setsockopt ( this->_socket, 
                              SOL_SOCKET, 
                              SO_REUSEADDR, 
                              &opt, 
                              sizeof(std::int32_t)) < 0 ) {
                return false;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(this->_port);

            if ( bind ( this->_socket, 
                        reinterpret_cast<sockaddr *>(&address), 
                        sizeof(sockaddr_in)) < 0 ) {
                return false;
            }

            if (::listen(this->_socket, 3) < 0) {
                return false;
            }

            auto address_length = socklen_t{sizeof(sockaddr_in)};
            fd_set socket_descriptors;
            auto max_socket_descriptor = this->_socket;
            std::int32_t socket_descriptor, client_socket;

            while (this->_is_running) {

                FD_ZERO(&socket_descriptors);
                FD_SET(this->_socket, &socket_descriptors);
                for (auto i = 0u; i < this->_client_sockets.size(); ++i) {
                    socket_descriptor = this->_client_sockets[i];
                    if (socket_descriptor > 0) {
                        FD_SET(socket_descriptor, &socket_descriptors);
                    }
                    max_socket_descriptor = std::max(max_socket_descriptor, socket_descriptor);
                }

                select(max_socket_descriptor + 1, &socket_descriptors, nullptr, nullptr, nullptr);
                if (FD_ISSET(this->_socket, &socket_descriptors)) {
                    if ((client_socket = accept(this->_socket, reinterpret_cast<sockaddr *>(&address), &address_length)) < 0) {
                        return false;
                    }
                    const auto id = this->add_client(client_socket);
                    if (id < this->_client_sockets.size()) {
                        this->_handler.on_client_connected(id);
                    }
                }

                for (auto i = 0u; i < this->_client_sockets.size(); i++) {
                    const auto client = this->_client_sockets[i];
                    if (FD_ISSET(client, &socket_descriptors)) {
                        if (read(client, this->_buffer.data(), this->_buffer.size()) == 0) {
                            ::close(client);
                            this->_client_sockets[i] = 0;
                            this->_handler.on_client_disconnected(i);
                        }
                        else {
                            std::string message(this->_buffer.data());
                            std::memset(this->_buffer.data(), '\0', message.length());
                            auto response = this->_handler.handle_message(i, message);
                            if (response.has_value()) {
                                this->send(i, std::move(response.value()));
                            }
                        }
                    }
                }
            }

            for (const auto client : this->_client_sockets) {
                if (client > 0) {
                    ::close(client);
                }
            }
            ::close(this->_socket);
            return true;
        }
        
        std::uint16_t add_client ( std::int32_t client_socket ) {
            for (auto i = 0u; i < this->_client_sockets.size(); i++)
            {
                if (this->_client_sockets[i] == 0)
                {
                    this->_client_sockets[i] = client_socket;
                    return i;
                }
            }
            return std::numeric_limits<std::uint16_t>::max();
        }
    
    private:
        const std::uint16_t _port;
        std::int32_t _socket = -1;
        std::array<std::uint32_t, 64> _client_sockets = { 0 };
        std::array<char, maxSizeClientMessage> _buffer;
        bool _is_running = true;
        Handler& _handler; 
    };
}


