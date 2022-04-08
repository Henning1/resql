#pragma once
#include <cstdint>
#include <string>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "def.h"
#include "util/ResqlError.h"


namespace network {

    class Client {

    public:

        Client ( std::string server, std::uint16_t port) 
            : _server ( server ), _port ( port ) {};

        ~Client() = default;
    
        bool connect() {
            this->_socket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (this->_socket < 0) {   
                return false;
            }
            
            struct addrinfo hints {
            }, *resource = nullptr;
            hints.ai_flags = AI_CANONNAME;
            hints.ai_family = AF_INET;
            hints.ai_socktype = 0;
            hints.ai_protocol = 0;
            
            const auto port = std::to_string(static_cast<std::int16_t>(this->_port));
            int result = ::getaddrinfo(this->_server.data(), port.c_str(), &hints, &resource);
            if (result == 0) {   
                auto *current = resource;
                while (current != nullptr) {   
                    if (::connect(this->_socket, current->ai_addr, current->ai_addrlen) == 0) {   
                        ::freeaddrinfo(resource);
                        return true;
                    }
                    current = current->ai_next;
                }
            }
            return false;
        }

        void disconnect() {
            ::close(this->_socket);
        }

        std::string send(const std::string& message) {
            if ( message.size() >= maxSizeClientMessage ) {
                throw ResqlError ( "Message too large (" + std::to_string ( message.size() ) 
                    + ">=" + std::to_string(maxSizeClientMessage) + ")" );
            } 
            const auto w = ::write(this->_socket, message.data(), message.size());
            if (w < 0) {   
                throw ResqlError ( "Error sending message." );
            }

            // Read header
            auto header = std::uint64_t(0);
            this->read_into_buffer(sizeof(header), static_cast<void *>(&header));

            // Read data
            auto message_buffer = std::string(header, '\0');
            this->read_into_buffer(header, message_buffer.data());

            return message_buffer;
        }
    
    
    private:
        const std::string    _server;
        const std::uint16_t  _port;
        std::int32_t         _socket = -1;
        
        std::uint64_t read_into_buffer(std::uint64_t length, void* buffer) {
            auto bytes_read = 0u;
            while (bytes_read < length) {
                const auto read =
                    ::read(this->_socket, reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(buffer) + bytes_read),
                           length - bytes_read);
                if (read < 1) {
                    return bytes_read;
                }
                bytes_read += read;
            }
            return bytes_read;
        }
    };
}

