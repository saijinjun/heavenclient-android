// Native (Android) backend for LazyFS: reads a complete local .nx file when
// there is one, and otherwise fetches 512 KB chunks over HTTP on first use,
// keeping them on disk so each piece is downloaded only once.
#include <LazyFS/LazyFileBackend.h>
#ifndef MS_PLATFORM_WASM

#include <LazyFS/HttpRange.h>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace LazyFS
{
	namespace
	{
		constexpr size_t META_HEADER = sizeof(uint64_t);
		// A dropped Wi-Fi packet should not blank a texture; try a few times.
		constexpr int FETCH_ATTEMPTS = 3;

		bool pread_all(int fd, void* buffer, size_t size, uint64_t offset)
		{
			auto out = static_cast<uint8_t*>(buffer);
			while (size > 0)
			{
				ssize_t n = ::pread(fd, out, size, static_cast<off_t>(offset));
				if (n <= 0)
				{
					return false;
				}
				out += n;
				size -= static_cast<size_t>(n);
				offset += static_cast<uint64_t>(n);
			}
			return true;
		}

		bool pwrite_all(int fd, const void* buffer, size_t size, uint64_t offset)
		{
			auto in = static_cast<const uint8_t*>(buffer);
			while (size > 0)
			{
				ssize_t n = ::pwrite(fd, in, size, static_cast<off_t>(offset));
				if (n <= 0)
				{
					return false;
				}
				in += n;
				size -= static_cast<size_t>(n);
				offset += static_cast<uint64_t>(n);
			}
			return true;
		}
	}

	LazyFileBackend::~LazyFileBackend()
	{
		close();
	}

	bool LazyFileBackend::open(const std::string& path)
	{
		close();
		path_ = path;
		current_offset_ = 0;

		is_open_ = open_local(path) || open_remote(path);
		if (!is_open_)
		{
			path_.clear();
		}
		return is_open_;
	}

	bool LazyFileBackend::open_local(const std::string& path)
	{
		int fd = ::open(path.c_str(), O_RDONLY);
		if (fd < 0)
		{
			return false;
		}

		struct stat info;
		if (::fstat(fd, &info) != 0 || info.st_size <= 0)
		{
			::close(fd);
			return false;
		}

		data_fd_ = fd;
		file_size_ = static_cast<size_t>(info.st_size);
		remote_ = false;
		return true;
	}

	bool LazyFileBackend::open_remote(const std::string& path)
	{
		if (remote_base().empty())
		{
			std::cerr << "[LazyFS] " << path << " is missing and no asset server is set" << std::endl;
			return false;
		}

		std::string cache = path + ".cache";
		std::string meta = path + ".lazy";

		int meta_fd = ::open(meta.c_str(), O_RDWR | O_CREAT, 0644);
		if (meta_fd < 0)
		{
			std::cerr << "[LazyFS] Cannot create " << meta << std::endl;
			return false;
		}

		uint64_t cached_size = 0;
		bool have_meta = pread_all(meta_fd, &cached_size, META_HEADER, 0) && cached_size > 0;

		// Ask the server every time, so a replaced file on the server (a new
		// UI.nx) resets the cache. Offline, the size recorded last time works.
		uint64_t size = 0;
		std::string error;
		if (!Http::head_size(remote_base() + path, size, error))
		{
			if (!have_meta)
			{
				std::cerr << "[LazyFS] " << error << std::endl;
				::close(meta_fd);
				return false;
			}
			size = cached_size;
		}

		size_t chunks = static_cast<size_t>((size + CHUNK_SIZE - 1) / CHUNK_SIZE);
		bool reset = !have_meta || cached_size != size;

		int data_fd = ::open(cache.c_str(), O_RDWR | O_CREAT | (reset ? O_TRUNC : 0), 0644);
		if (data_fd < 0 || ::ftruncate(data_fd, static_cast<off_t>(size)) != 0)
		{
			std::cerr << "[LazyFS] Cannot create " << cache << std::endl;
			if (data_fd >= 0)
			{
				::close(data_fd);
			}
			::close(meta_fd);
			return false;
		}

		present_.assign(chunks, 0);
		if (reset)
		{
			// Size first, then an all-zero chunk map: nothing is cached yet.
			if (::ftruncate(meta_fd, 0) != 0 ||
				!pwrite_all(meta_fd, &size, META_HEADER, 0) ||
				!pwrite_all(meta_fd, present_.data(), chunks, META_HEADER))
			{
				std::cerr << "[LazyFS] Cannot write " << meta << std::endl;
				::close(data_fd);
				::close(meta_fd);
				return false;
			}
		}
		else if (!pread_all(meta_fd, present_.data(), chunks, META_HEADER))
		{
			present_.assign(chunks, 0);
		}

		present_count_ = 0;
		for (uint8_t flag : present_)
		{
			present_count_ += flag ? 1 : 0;
		}

		data_fd_ = data_fd;
		meta_fd_ = meta_fd;
		file_size_ = static_cast<size_t>(size);
		remote_ = true;
		finish_if_complete();
		return true;
	}

	void LazyFileBackend::close()
	{
		if (data_fd_ >= 0)
		{
			::close(data_fd_);
		}
		if (meta_fd_ >= 0)
		{
			::close(meta_fd_);
		}
		data_fd_ = -1;
		meta_fd_ = -1;
		remote_ = false;
		present_.clear();
		present_count_ = 0;
		is_open_ = false;
		path_.clear();
		file_size_ = 0;
		current_offset_ = 0;
	}

	size_t LazyFileBackend::read(void* buffer, size_t size)
	{
		if (!is_open_)
		{
			return 0;
		}

		size_t to_read = std::min(size, file_size_ - current_offset_);
		if (to_read == 0 || !fetch_data(current_offset_, to_read, buffer))
		{
			return 0;
		}

		current_offset_ += to_read;
		return to_read;
	}

	bool LazyFileBackend::seek(size_t offset)
	{
		if (!is_open_ || offset > file_size_)
		{
			return false;
		}
		current_offset_ = offset;
		return true;
	}

	bool LazyFileBackend::fetch_data(size_t offset, size_t size, void* out_buffer)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (remote_ && !ensure_chunks(offset / CHUNK_SIZE, (offset + size - 1) / CHUNK_SIZE))
		{
			return false;
		}

		return pread_all(data_fd_, out_buffer, size, offset);
	}

	bool LazyFileBackend::ensure_chunks(size_t first, size_t last)
	{
		for (size_t index = first; index <= last && index < present_.size(); index++)
		{
			if (present_[index])
			{
				continue;
			}

			uint64_t start = static_cast<uint64_t>(index) * CHUNK_SIZE;
			size_t length = static_cast<size_t>(std::min<uint64_t>(CHUNK_SIZE, file_size_ - start));
			std::vector<uint8_t> data;
			std::string error;
			bool fetched = false;

			for (int attempt = 0; attempt < FETCH_ATTEMPTS && !fetched; attempt++)
			{
				fetched = Http::get_range(remote_base() + path_, start, length, data, error);
			}

			if (!fetched)
			{
				std::cerr << "[LazyFS] " << error << std::endl;
				return false;
			}

			// Data first, then the flag, so a crash in between only means
			// downloading the chunk again.
			uint8_t flag = 1;
			if (!pwrite_all(data_fd_, data.data(), length, start) ||
				!pwrite_all(meta_fd_, &flag, 1, META_HEADER + index))
			{
				std::cerr << "[LazyFS] Cannot save part of " << path_ << " (storage full?)" << std::endl;
				return false;
			}

			present_[index] = 1;
			present_count_++;
		}

		finish_if_complete();
		return true;
	}

	void LazyFileBackend::finish_if_complete()
	{
		if (!remote_ || present_count_ != present_.size())
		{
			return;
		}

		// Every chunk is here: promote the cache to a normal local file, so
		// later starts read it directly and never contact the server for it.
		// The open descriptor keeps working across the rename.
		std::string cache = path_ + ".cache";
		std::string meta = path_ + ".lazy";
		if (std::rename(cache.c_str(), path_.c_str()) == 0)
		{
			::close(meta_fd_);
			meta_fd_ = -1;
			std::remove(meta.c_str());
			remote_ = false;
		}
	}
}

#endif
