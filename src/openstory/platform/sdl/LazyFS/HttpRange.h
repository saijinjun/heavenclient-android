#pragma once
// Minimal HTTP/1.1 client for native builds: just enough to ask a plain-HTTP
// file server (web/server.py) for a file's size and for byte ranges of it.
#ifndef MS_PLATFORM_WASM

#include <cstdint>
#include <string>
#include <vector>

namespace LazyFS
{
	namespace Http
	{
		// Size of the file at url, from a HEAD request.
		bool head_size(const std::string& url, uint64_t& size, std::string& error);

		// Bytes [offset, offset + length) of the file at url, exactly.
		bool get_range(const std::string& url, uint64_t offset, size_t length,
			std::vector<uint8_t>& out, std::string& error);
	}

	// Base URL that remote .nx files are fetched from, ending in a slash,
	// e.g. http://100.91.238.34:8090/assets/. Empty disables streaming.
	void set_remote_base(const std::string& url);
	const std::string& remote_base();
}

#endif
