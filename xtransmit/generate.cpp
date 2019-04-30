#pragma once
#include <memory>
#include <vector>
#include "srt_socket.hpp"



using namespace std;
using socket_ptr = std::shared_ptr<xtransmit::srt::socket>;

//std::vector<std::future<void>> accepting_threads;


struct generate_config
{
	int bitrate;
	int num_messages;
	int message_size;
};


void generate(socket_ptr dst, const generate_config &cfg,
	const atomic_bool& force_break)
{
	// 1. Wait for acccept
	//auto s_accepted = dst->async_accept();
	// catch exception (error)

	// if ok start another async_accept
	//accepting_threads.push_back(
	//	async(std::launch::async, &async_accept, s)
	//);

	

	auto time_prev = chrono::steady_clock::now();
	long time_dev_us = 0;
	const long msgs_per_s = static_cast<long long>(cfg.bitrate / 8) / cfg.message_size;
	const long msg_interval_us = msgs_per_s ? 1000000 / msgs_per_s : 0;

	for (int i = 0; (i < cfg.num_messages) && !force_break; ++i)
	{
		if (cfg.bitrate)
		{
			const long duration_us = time_dev_us > msg_interval_us ? 0 : (msg_interval_us - time_dev_us);
			const auto next_time = time_prev + chrono::microseconds(duration_us);
			chrono::time_point<chrono::steady_clock> time_now;
			for (;;)
			{
				time_now = chrono::steady_clock::now();
				if (time_now >= next_time)
					break;
				if (force_break)
					break;
			}

			time_dev_us += (long)chrono::duration_cast<chrono::microseconds>(time_now - time_prev).count() - msg_interval_us;
			time_prev = time_now;
		}

		dst->write();
	}
}




void start_generator(future<socket_ptr> &connection, const generate_config& cfg,
	const atomic_bool& force_break)
{
	const socket_ptr sock = connection.get();

	generate(sock, cfg, force_break);
}



void establish_connection()
{

}
