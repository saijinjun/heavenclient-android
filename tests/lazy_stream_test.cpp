#include <doctest/doctest.h>
#include "client/LazyFS/HttpRange.h"
#include "client/LazyFS/LazyFileBackend.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    // A file server for one file, like web/server.py: HEAD gives the size and
    // GET honours "Range: bytes=a-b". Counts the GETs so tests can tell what
    // came from the network and what from the on-disk cache.
    class RangeServer
    {
    public:
        explicit RangeServer(std::vector<uint8_t> body) : body_(std::move(body))
        {
            listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
            int yes = 1;
            ::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;
            REQUIRE(::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
            REQUIRE(::listen(listener_, 8) == 0);
            socklen_t length = sizeof(address);
            ::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length);
            port_ = ntohs(address.sin_port);
            thread_ = std::thread([this] { serve(); });
        }

        ~RangeServer()
        {
            stopping_ = true;
            ::shutdown(listener_, SHUT_RDWR);
            ::close(listener_);
            thread_.join();
        }

        std::string base() const
        {
            return "http://127.0.0.1:" + std::to_string(port_) + "/assets/";
        }

        int gets() const { return gets_; }

    private:
        void serve()
        {
            while (!stopping_)
            {
                int client = ::accept(listener_, nullptr, nullptr);
                if (client < 0)
                {
                    return;
                }
                handle(client);
                ::close(client);
            }
        }

        void handle(int client)
        {
            std::string request;
            char buffer[1024];
            while (request.find("\r\n\r\n") == std::string::npos)
            {
                ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
                if (n <= 0)
                {
                    return;
                }
                request.append(buffer, static_cast<size_t>(n));
            }

            bool head = request.compare(0, 5, "HEAD ") == 0;
            std::string reply;
            if (head)
            {
                reply = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body_.size()) + "\r\n\r\n";
                send_all(client, reply.data(), reply.size());
                return;
            }

            gets_++;
            size_t first = 0;
            size_t last = 0;
            size_t at = request.find("Range: bytes=");
            REQUIRE(at != std::string::npos);
            REQUIRE(std::sscanf(request.c_str() + at, "Range: bytes=%zu-%zu", &first, &last) == 2);
            last = std::min(last, body_.size() - 1);
            size_t length = last - first + 1;
            reply = "HTTP/1.1 206 Partial Content\r\nContent-Length: " + std::to_string(length) +
                "\r\nContent-Range: bytes " + std::to_string(first) + "-" + std::to_string(last) +
                "/" + std::to_string(body_.size()) + "\r\n\r\n";
            send_all(client, reply.data(), reply.size());
            send_all(client, body_.data() + first, length);
        }

        static void send_all(int client, const void* data, size_t size)
        {
            auto bytes = static_cast<const char*>(data);
            while (size > 0)
            {
                ssize_t n = ::send(client, bytes, size, MSG_NOSIGNAL);
                if (n <= 0)
                {
                    return;
                }
                bytes += n;
                size -= static_cast<size_t>(n);
            }
        }

        std::vector<uint8_t> body_;
        int listener_ = -1;
        uint16_t port_ = 0;
        std::thread thread_;
        std::atomic<bool> stopping_{ false };
        std::atomic<int> gets_{ 0 };
    };

    constexpr size_t CHUNK = LazyFS::LazyFileBackend::CHUNK_SIZE;

    std::vector<uint8_t> pattern(size_t size)
    {
        std::vector<uint8_t> bytes(size);
        for (size_t i = 0; i < size; i++)
        {
            bytes[i] = static_cast<uint8_t>((i * 2654435761u) >> 13);
        }
        return bytes;
    }

    std::vector<uint8_t> read_at(LazyFS::LazyFileBackend& file, size_t offset, size_t size)
    {
        std::vector<uint8_t> out(size);
        REQUIRE(file.seek(offset));
        REQUIRE(file.read(out.data(), size) == size);
        return out;
    }

    bool exists(const char* path)
    {
        return std::ifstream{ path }.good();
    }

    // The work folder is reused between runs; start each case from nothing.
    void forget(const std::string& name)
    {
        for (const char* suffix : { "", ".cache", ".lazy" })
        {
            std::remove((name + suffix).c_str());
        }
    }
}

TEST_CASE("A missing file is fetched piece by piece and each piece only once")
{
    forget("Test.nx");
    // Three and a half chunks, so the last one is short.
    std::vector<uint8_t> body = pattern(3 * CHUNK + CHUNK / 2);
    RangeServer server(body);
    LazyFS::set_remote_base(server.base());

    LazyFS::LazyFileBackend file;
    REQUIRE(file.open("Test.nx"));
    CHECK(file.size() == body.size());
    CHECK(server.gets() == 0);

    // A read across the first chunk boundary needs chunks 0 and 1.
    size_t offset = CHUNK - 100;
    CHECK(read_at(file, offset, 200) == std::vector<uint8_t>(body.begin() + offset, body.begin() + offset + 200));
    CHECK(server.gets() == 2);

    // The same bytes again come from the phone, not the server.
    read_at(file, offset, 200);
    CHECK(server.gets() == 2);
    CHECK(exists("Test.nx.cache"));
    CHECK_FALSE(exists("Test.nx"));
}

TEST_CASE("Pieces fetched before a restart are kept")
{
    forget("Kept.nx");
    std::vector<uint8_t> body = pattern(2 * CHUNK + 10);
    RangeServer server(body);
    LazyFS::set_remote_base(server.base());

    {
        LazyFS::LazyFileBackend file;
        REQUIRE(file.open("Kept.nx"));
        read_at(file, 0, 16);
        CHECK(server.gets() == 1);
    }

    LazyFS::LazyFileBackend again;
    REQUIRE(again.open("Kept.nx"));
    CHECK(read_at(again, 0, 16) == std::vector<uint8_t>(body.begin(), body.begin() + 16));
    CHECK(server.gets() == 1);
}

TEST_CASE("A fully fetched file becomes a normal local file")
{
    forget("Whole.nx");
    std::vector<uint8_t> body = pattern(CHUNK + 1);
    RangeServer server(body);
    LazyFS::set_remote_base(server.base());

    {
        LazyFS::LazyFileBackend file;
        REQUIRE(file.open("Whole.nx"));
        CHECK(read_at(file, 0, body.size()) == body);
    }

    CHECK(exists("Whole.nx"));
    CHECK_FALSE(exists("Whole.nx.cache"));
    CHECK_FALSE(exists("Whole.nx.lazy"));

    // Later starts read it directly, even with the server gone.
    LazyFS::set_remote_base("");
    LazyFS::LazyFileBackend local;
    REQUIRE(local.open("Whole.nx"));
    CHECK(read_at(local, CHUNK - 3, 4) == std::vector<uint8_t>(body.end() - 4, body.end()));
    CHECK(server.gets() == 2);
}

TEST_CASE("A file changed on the server starts its cache over")
{
    forget("Changed.nx");
    {
        RangeServer server(pattern(CHUNK + 5));
        LazyFS::set_remote_base(server.base());
        LazyFS::LazyFileBackend file;
        REQUIRE(file.open("Changed.nx"));
        read_at(file, 0, 8);
    }

    std::vector<uint8_t> replaced(CHUNK + 9, 0x5a);
    RangeServer server(replaced);
    LazyFS::set_remote_base(server.base());
    LazyFS::LazyFileBackend file;
    REQUIRE(file.open("Changed.nx"));
    CHECK(file.size() == replaced.size());
    CHECK(read_at(file, 0, 8) == std::vector<uint8_t>(8, 0x5a));
    CHECK(server.gets() == 1);
}

TEST_CASE("Without a local copy or a server, opening fails")
{
    forget("Nowhere.nx");
    LazyFS::set_remote_base("");
    LazyFS::LazyFileBackend file;
    CHECK_FALSE(file.open("Nowhere.nx"));
}
