#pragma once

#include "server.h"
#include "../execute.h"


namespace network {

    class Handler {
    public:
        Handler() noexcept = default;
        virtual ~Handler() noexcept = default;
        virtual std::optional<std::string> handle_message(const std::uint32_t client_id, const std::string& message) = 0;
        virtual void on_client_connected(const std::uint32_t id) = 0;
        virtual void on_client_disconnected(const std::uint32_t id) = 0;
    };

}


class ResqlHandler final : public network::Handler {

public:

    ResqlHandler(Database& db, DBConfig& config) 
        : _db(db), _config(config) {}

    ~ResqlHandler() = default;

    std::optional<std::string> handle_message ( 
        const std::uint32_t client_id, 
        const std::string& message) override {

        QueryResult res = executeStatement ( message, _db, _config );
        std::cout << "error: " << res.error << ", error message: " << res.errorMessage << std::endl;
        
        std::stringstream response ( std::ios::binary | std::ios::out );
        cereal::BinaryOutputArchive archiver ( response );
        archiver ( res );
        return std::make_optional ( response.str() );
    }

    void on_client_connected(const std::uint32_t id) override { }

    void on_client_disconnected(const std::uint32_t id) override { }

    Database& _db;
    DBConfig& _config;
};

