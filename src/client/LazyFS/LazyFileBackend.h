#pragma once

#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#include <emscripten/val.h>
#else
#include <cstdint>
#include <mutex>
#include <vector>
#endif
#include <memory>
#include <string>

namespace LazyFS
{

	/**
	 * LazyFileBackend - Lazy loading file access using HTTP Range requests
	 *
	 * This class provides on-demand file chunk loading from a remote backend server.
	 * Instead of loading entire files into memory, it fetches chunks as needed using
	 * HTTP Range requests through the JavaScript bridge layer (lazy_fs.js).
	 *
	 * Key features:
	 * - Minimum 10MB chunk size for efficient network usage
	 * - In-memory chunk caching (JavaScript side)
	 * - Supports read(), seek(), and stat() operations
	 * - Compatible with mmap-style random access patterns
	 */
	class LazyFileBackend
	{
	  public:
		// 64KB chunks - with WebSocket binary frames, per-chunk overhead is ~15 bytes
		// Small chunks = less wasted data for small reads (like NX headers)
#ifdef MS_PLATFORM_WASM
		static constexpr size_t CHUNK_SIZE = 2048 * 1024;
#else
		// Native builds keep fetched pieces on disk, so a chunk is also the
		// unit of download. Small enough that browsing a node table does not
		// pull megabytes, large enough that a bitmap is a few requests.
		static constexpr size_t CHUNK_SIZE = 512 * 1024;
#endif

		/**
		 * Open a file for lazy loading
		 * @param path Path to the file (relative to web server root)
		 * @return true if file was successfully opened, false otherwise
		 */
		bool open(const std::string& path);

		/**
		 * Close the file and release resources
		 */
		void close();

		/**
		 * Read data from file at current position
		 * @param buffer Output buffer
		 * @param size Number of bytes to read
		 * @return Number of bytes actually read
		 */
		size_t read(void* buffer, size_t size);

		/**
		 * Seek to a specific position in the file
		 * @param offset Byte offset from beginning of file
		 * @return true if seek was successful
		 */
		bool seek(size_t offset);

		/**
		 * Get current position in file
		 */
		size_t tell() const
		{
			return current_offset_;
		}

		/**
		 * Get file size
		 */
		size_t size() const
		{
			return file_size_;
		}

		/**
		 * Check if file is open
		 */
		bool is_open() const
		{
			return is_open_;
		}

		/**
		 * Get the file path
		 */
		const std::string& path() const
		{
			return path_;
		}

	  private:
		std::string path_;
		size_t file_size_ = 0;
		size_t current_offset_ = 0;
		bool is_open_ = false;

		/**
		 * Fetch data from the server (calls JavaScript)
		 * @param offset Byte offset in file
		 * @param size Number of bytes to fetch
		 * @param out_buffer Output buffer
		 * @return true if fetch was successful
		 */
		bool fetch_data(size_t offset, size_t size, void* out_buffer);

#ifndef MS_PLATFORM_WASM
		// Native: a complete local copy is read directly. Otherwise the file
		// is mirrored into <name>.cache, a sparse file the size of the
		// original, and <name>.lazy records which chunks are already there:
		// 8 bytes of file size, then one byte per chunk.
		bool open_local(const std::string& path);
		bool open_remote(const std::string& path);
		bool ensure_chunks(size_t first, size_t last);
		void finish_if_complete();

		int data_fd_ = -1;
		int meta_fd_ = -1;
		bool remote_ = false;
		std::vector<uint8_t> present_;
		size_t present_count_ = 0;
		std::mutex mutex_;
	  public:
		~LazyFileBackend();
#endif
	};

	/**
	 * LazyFile - RAII wrapper for LazyFileBackend
	 */
	class LazyFile
	{
	  public:
		LazyFile() = default;
		explicit LazyFile(const std::string& path);
		~LazyFile();

		// Disable copy, enable move
		LazyFile(const LazyFile&) = delete;
		LazyFile& operator=(const LazyFile&) = delete;
		LazyFile(LazyFile&&) noexcept;
		LazyFile& operator=(LazyFile&&) noexcept;

		bool open(const std::string& path);
		void close();
		size_t read(void* buffer, size_t size);
		bool seek(size_t offset);
		size_t tell() const;
		size_t size() const;
		bool is_open() const;
		const std::string& path() const;

		// Get underlying backend (for advanced usage)
		LazyFileBackend* backend()
		{
			return backend_.get();
		}

	  private:
		std::unique_ptr<LazyFileBackend> backend_;
	};

} // namespace LazyFS
