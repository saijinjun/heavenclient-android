#include <LazyFS/HttpRange.h>
#ifndef MS_PLATFORM_WASM

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace LazyFS
{
	namespace
	{
		constexpr int CONNECT_TIMEOUT_MS = 8000;
		constexpr int READ_TIMEOUT_MS = 30000;

		std::string base_url;

		struct Url
		{
			std::string host;
			std::string port;
			std::string path;
		};

		bool parse_url(const std::string& url, Url& out)
		{
			const std::string scheme = "http://";
			if (url.compare(0, scheme.size(), scheme) != 0)
			{
				return false;
			}

			size_t host_start = scheme.size();
			size_t path_start = url.find('/', host_start);
			std::string authority = url.substr(host_start, path_start - host_start);
			out.path = path_start == std::string::npos ? "/" : url.substr(path_start);

			size_t colon = authority.rfind(':');
			if (colon == std::string::npos)
			{
				out.host = authority;
				out.port = "80";
			}
			else
			{
				out.host = authority.substr(0, colon);
				out.port = authority.substr(colon + 1);
			}

			return !out.host.empty() && !out.port.empty();
		}

		// Waits until fd is ready for the given events, or the timeout passes.
		bool wait_for(int fd, short events, int timeout_ms)
		{
			pollfd entry{ fd, events, 0 };
			int result;
			do
			{
				result = ::poll(&entry, 1, timeout_ms);
			} while (result < 0 && errno == EINTR);
			return result > 0;
		}

		int connect_to(const Url& url, std::string& error)
		{
			addrinfo hints{};
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			addrinfo* found = nullptr;

			if (int code = ::getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &found))
			{
				error = "cannot resolve " + url.host + ": " + gai_strerror(code);
				return -1;
			}

			int fd = -1;
			for (addrinfo* ai = found; ai; ai = ai->ai_next)
			{
				fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
				if (fd < 0)
				{
					continue;
				}

				// Non-blocking connect so an unreachable server (Tailscale off)
				// fails after a few seconds instead of the OS's minutes.
				int flags = ::fcntl(fd, F_GETFL, 0);
				::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
				int result = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
				bool connected = result == 0;

				if (!connected && errno == EINPROGRESS && wait_for(fd, POLLOUT, CONNECT_TIMEOUT_MS))
				{
					int socket_error = 0;
					socklen_t length = sizeof(socket_error);
					::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length);
					connected = socket_error == 0;
				}

				if (connected)
				{
					::fcntl(fd, F_SETFL, flags);
					break;
				}

				::close(fd);
				fd = -1;
			}

			::freeaddrinfo(found);

			if (fd < 0)
			{
				error = "cannot connect to " + url.host + ":" + url.port;
			}
			return fd;
		}

		bool send_all(int fd, const std::string& data)
		{
			size_t sent = 0;
			while (sent < data.size())
			{
				ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
				if (n <= 0)
				{
					if (n < 0 && errno == EINTR)
					{
						continue;
					}
					return false;
				}
				sent += static_cast<size_t>(n);
			}
			return true;
		}

		struct Response
		{
			int status = 0;
			int64_t content_length = -1;
			std::vector<uint8_t> body;
		};

		// Sends one request and reads the reply until the server closes the
		// connection (requests say Connection: close).
		bool request(const std::string& method, const std::string& url, const std::string& extra_headers,
			Response& response, std::string& error)
		{
			Url parsed;
			if (!parse_url(url, parsed))
			{
				error = "unsupported URL " + url;
				return false;
			}

			int fd = connect_to(parsed, error);
			if (fd < 0)
			{
				return false;
			}

			std::string head = method + " " + parsed.path + " HTTP/1.1\r\n"
				"Host: " + parsed.host + ":" + parsed.port + "\r\n"
				"Accept-Encoding: identity\r\n"
				"Connection: close\r\n" + extra_headers + "\r\n";

			if (!send_all(fd, head))
			{
				::close(fd);
				error = "connection lost while sending";
				return false;
			}

			std::vector<uint8_t> raw;
			uint8_t buffer[64 * 1024];
			while (true)
			{
				if (!wait_for(fd, POLLIN, READ_TIMEOUT_MS))
				{
					::close(fd);
					error = "the server stopped responding";
					return false;
				}

				ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);
				if (n < 0 && errno == EINTR)
				{
					continue;
				}
				if (n <= 0)
				{
					break;
				}
				raw.insert(raw.end(), buffer, buffer + n);
			}
			::close(fd);

			const char separator[] = "\r\n\r\n";
			auto header_end = std::search(raw.begin(), raw.end(), separator, separator + 4);
			if (header_end == raw.end())
			{
				error = "malformed reply from the server";
				return false;
			}

			std::string headers(raw.begin(), header_end);
			if (std::sscanf(headers.c_str(), "HTTP/%*d.%*d %d", &response.status) != 1)
			{
				error = "malformed status line from the server";
				return false;
			}

			// Header names are case-insensitive.
			std::string lower = headers;
			for (char& c : lower)
			{
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			size_t length_at = lower.find("\r\ncontent-length:");
			if (length_at != std::string::npos)
			{
				response.content_length = std::strtoll(headers.c_str() + length_at + 17, nullptr, 10);
			}

			response.body.assign(header_end + 4, raw.end());
			return true;
		}
	}

	namespace Http
	{
		bool head_size(const std::string& url, uint64_t& size, std::string& error)
		{
			Response response;
			if (!request("HEAD", url, "", response, error))
			{
				return false;
			}
			if (response.status != 200 || response.content_length <= 0)
			{
				error = "the server has no " + url + " (HTTP " + std::to_string(response.status) + ")";
				return false;
			}
			size = static_cast<uint64_t>(response.content_length);
			return true;
		}

		bool get_range(const std::string& url, uint64_t offset, size_t length,
			std::vector<uint8_t>& out, std::string& error)
		{
			Response response;
			std::string range = "Range: bytes=" + std::to_string(offset) + "-" +
				std::to_string(offset + length - 1) + "\r\n";

			if (!request("GET", url, range, response, error))
			{
				return false;
			}

			if (response.status == 206 && response.body.size() == length)
			{
				out = std::move(response.body);
				return true;
			}

			// A 200 means the server ignored Range and is sending the whole
			// file, possibly gigabytes; refuse rather than buffer it.
			error = "unexpected reply for " + url + " (HTTP " + std::to_string(response.status) +
				", " + std::to_string(response.body.size()) + " of " + std::to_string(length) + " bytes)";
			return false;
		}
	}

	void set_remote_base(const std::string& url)
	{
		base_url = url;
		if (!base_url.empty() && base_url.back() != '/')
		{
			base_url += '/';
		}
	}

	const std::string& remote_base()
	{
		return base_url;
	}
}

#endif
