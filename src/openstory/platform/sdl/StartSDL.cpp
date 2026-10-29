//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "StartSDL.h"

#include <iostream>

#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <unwind.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#endif

namespace ms
{
	namespace sdl
	{
		namespace
		{
			std::string data_directory;

#ifdef __ANDROID__
			// Crash reports: a crash writes crash.txt (signal, exception and
			// the call stack with function names) into the data folder, and
			// the next start shows it so it can be screenshotted.
			constexpr const char* CRASH_FILE = "crash.txt";
			int crash_fd = -1;
			char crash_stack[64 * 1024];

			struct Frames
			{
				void* pcs[48];
				size_t count;
			};

			_Unwind_Reason_Code collect(struct _Unwind_Context* context, void* arg)
			{
				Frames* frames = static_cast<Frames*>(arg);
				uintptr_t pc = _Unwind_GetIP(context);

				if (pc && frames->count < 48)
					frames->pcs[frames->count++] = reinterpret_cast<void*>(pc);

				return frames->count < 48 ? _URC_NO_REASON : _URC_END_OF_STACK;
			}

			void write_line(const char* text)
			{
				if (crash_fd >= 0)
				{
					ssize_t ignored = ::write(crash_fd, text, std::strlen(text));
					(void)ignored;
				}

				__android_log_write(ANDROID_LOG_ERROR, "OpenStory", text);
			}

			void write_stack()
			{
				Frames frames = {};
				_Unwind_Backtrace(collect, &frames);

				char line[512];

				for (size_t i = 0; i < frames.count; i++)
				{
					Dl_info info = {};
					const char* name = nullptr;
					char* demangled = nullptr;

					if (dladdr(frames.pcs[i], &info) && info.dli_sname)
					{
						int status = 0;
						demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
						name = demangled ? demangled : info.dli_sname;
					}

					const char* lib = info.dli_fname ? std::strrchr(info.dli_fname, '/') : nullptr;
					uintptr_t offset = reinterpret_cast<uintptr_t>(frames.pcs[i]) - reinterpret_cast<uintptr_t>(info.dli_fbase);

					std::snprintf(line, sizeof(line), "#%02zu %s+0x%zx %.300s\n",
						i, lib ? lib + 1 : "?", static_cast<size_t>(offset), name ? name : "");
					write_line(line);
					std::free(demangled);
				}
			}

			void on_crash(int sig, siginfo_t* info, void*)
			{
				char line[128];
				std::snprintf(line, sizeof(line), "Crash: signal %d (%s), address %p\n",
					sig, strsignal(sig), info ? info->si_addr : nullptr);
				write_line(line);
				write_stack();

				if (crash_fd >= 0)
					::close(crash_fd);

				::signal(sig, SIG_DFL);
				::raise(sig);
			}

			void on_terminate()
			{
				write_line("Crash: uncaught exception");

				if (std::exception_ptr error = std::current_exception())
				{
					try
					{
						std::rethrow_exception(error);
					}
					catch (const std::exception& e)
					{
						write_line(": ");
						write_line(e.what());
					}
					catch (...)
					{
					}
				}

				write_line("\n");
				std::abort();
			}

			void install_crash_handler()
			{
				crash_fd = ::open(CRASH_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				stack_t stack = {};
				stack.ss_sp = crash_stack;
				stack.ss_size = sizeof(crash_stack);
				::sigaltstack(&stack, nullptr);

				struct sigaction action = {};
				action.sa_sigaction = on_crash;
				action.sa_flags = SA_SIGINFO | SA_ONSTACK;
				sigemptyset(&action.sa_mask);

				for (int sig : { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL })
					::sigaction(sig, &action, nullptr);

				std::set_terminate(on_terminate);
			}

			void show_last_crash()
			{
				std::ifstream file(CRASH_FILE);

				if (!file)
					return;

				std::stringstream text;
				text << file.rdbuf();
				file.close();

				std::string report = text.str();

				if (report.empty())
					return;

				std::rename(CRASH_FILE, "crash_last.txt");

				if (report.size() > 3000)
					report = report.substr(0, 3000);

				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
					"OpenStory crashed last time - please screenshot this",
					report.c_str(), nullptr);
			}
#endif
		}

		std::string prepare_data_directory(int argc, char* argv[])
		{
			std::string directory;

#ifdef __ANDROID__
			(void)argc;
			(void)argv;
			const char* external = SDL_AndroidGetExternalStoragePath();
			directory = external ? external : "";
#else
			if (argc > 1)
				directory = argv[1];
#endif

			if (!directory.empty() && ::chdir(directory.c_str()) != 0)
				std::cerr << "[Init] Could not open data folder " << directory << std::endl;

			char buffer[4096];
			data_directory = ::getcwd(buffer, sizeof(buffer)) ? buffer : directory;

#ifdef __ANDROID__
			show_last_crash();
			install_crash_handler();
#endif

			return data_directory;
		}

		void show_error(const std::string& message)
		{
			std::string text = message;
			text += "\n\nGame files and the Settings file go in:\n" + data_directory;
			text += "\n\nThe server address is ServerIP in Settings.";

			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenStory", text.c_str(), nullptr);
		}
	}
}
