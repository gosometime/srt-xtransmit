#pragma once
#include <atomic>
#include <string>


//#define ENABLE_FILE
#ifdef ENABLE_FILE

namespace xtransmit {
	namespace file {

		struct config
		{
			std::string src_path;
			size_t      segment_size = 1456 * 1000;
			bool        only_print = false;	// Do not transfer, just enumerate files and print to stdout
		};


		void send(const std::string& dst_url, const config& cfg,
			const std::atomic_bool& force_break);


	} // namespace generate
} // namespace xtransmit

#endif
