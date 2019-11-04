#ifdef ENABLE_FILE
#include <iostream>
#include <iterator>
#include <filesystem>	// Requires C++17
#include <functional>
#include <string>
#include <vector>
#include <deque>

#include "sendfile.h"
#include "srt_socket.hpp"


using namespace std;
using namespace xtransmit;
using namespace xtransmit::file;
namespace fs = std::filesystem;


using shared_srt_socket = std::shared_ptr<srt::socket>;


/// Sendt one file in the messaging mode.
/// @return true on success, false if an error happened during transmission
bool send_file(const string &filename, const string &upload_name, srt::socket &dst
	, vector<char> &buf, const atomic_bool& force_break)
{
	ifstream ifile(filename, ios::binary);
	if (!ifile)
	{
		cerr << "Error opening file : " << filename << endl;
		return false;
	}

	const chrono::steady_clock::time_point time_start = chrono::steady_clock::now();
	size_t file_size = 0;

	cerr << "Transmitting '" << filename << " to " << upload_name;

	/*   1 byte      string    1 byte
	 * ------------------------------------------------
	 * | ......EF | Filename | 0     | Payload
	 * ------------------------------------------------
	 * E - enf of file flag
	 * F - frist sefment of a file (flag)
	 * We add +2 to include the first byte and the \0-character
	 */
	int hdr_size = snprintf(buf.data() + 1, buf.size(), "%s", upload_name.c_str()) + 2;

	while (!force_break)
	{
		const int n = (int)ifile.read(buf.data() + hdr_size, streamsize(buf.size() - hdr_size)).gcount();
		const bool is_eof = ifile.eof();
		const bool is_start = hdr_size > 1;
		buf[0] = (is_eof ? 2 : 0) | (is_start ? 1 : 0);

		size_t shift = 0;
		if (n > 0)
		{
			const int st = dst.write(const_buffer(buf.data() + shift, n + hdr_size));
			file_size += n;

			if (st == SRT_ERROR)
			{
				cerr << "Upload: SRT error: " << srt_getlasterror_str() << endl;
				return false;
			}
			if (st != n + hdr_size)
			{
				cerr << "Upload error: not full delivery" << endl;
				return false;
			}

			shift += st - hdr_size;
			hdr_size = 1;
		}

		if (is_eof)
			break;

		if (!ifile.good())
		{
			cerr << "ERROR while reading from file\n";
			return false;
		}
	}

	const chrono::steady_clock::time_point time_end = chrono::steady_clock::now();
	const auto delta_us = chrono::duration_cast<chrono::microseconds>(time_end - time_start).count();

	const size_t rate_kbps = (file_size * 1000) / (delta_us) * 8;
	cerr << "--> done (" << file_size / 1024 << " kbytes transfered at " << rate_kbps << " kbps, took "
		<< chrono::duration_cast<chrono::seconds>(time_end - time_start).count() << " s" << endl;

	return true;
}


/// Enumerate files in the folder and subfolders. Or return file if path is file.
/// With NRVO no copy of the vector being returned will be made
/// @param path    a path to a file or folder to enumerate
/// @return        a list of filenames found in the path
const std::vector<string> read_directory(const string& path)
{
	vector<string> filenames;
	deque<string> subdirs = { path };

	while (!subdirs.empty())
	{
		fs::path p(subdirs.front());
		subdirs.pop_front();

		if (!fs::is_directory(p))
		{
			filenames.push_back(p.string());
			continue;
		}

		for (const auto& entry : filesystem::directory_iterator(p))
		{
			if (entry.is_directory())
				subdirs.push_back(entry.path().string());
			else
				filenames.push_back(entry.path().string());
		}
	}

	return filenames;
}


/// Get file path relative to root directory.
/// Transmission preserves only relative dir structure.
///
/// @return    filename if filepath matches dirpath (dirpath is a file)
///            relative file path if dirpath is a folder
const string relative_path(const string& filepath, const string &dirpath)
{
	const fs::path dir(dirpath);
	const fs::path file(filepath);
	if (dir == file)
		return file.filename().string();

	const size_t pos = file.string().find(dir.string());
	if (pos != 0)
	{
		cerr << "Failed to find substring" << endl;
		return string();
	}

	return file.generic_string().erase(pos, dir.generic_string().size());
	//file.filename().string().replace(;
}


void start_filesender(future<shared_srt_socket> connection, const config& cfg,
	const vector<string> &filenames, const atomic_bool& force_break)
{
	if (!connection.valid())
	{
		cerr << "Error: Unexpected socket creation failure!" << endl;
		return;
	}

	const shared_srt_socket sock = connection.get();
	if (!sock)
	{
		cerr << "Error: Unexpected socket connection failure!" << endl;
		return;
	}

	srt::socket dst_sock = *sock.get();

	vector<char> buf(cfg.segment_size);
	//using namespace placeholders;
	//auto send = std::bind(send_file, _1, _1, dst_sock, buf);
	//for_each(filenames.begin(), filenames.end(), send);

	//for (const string& filename : filenames)
	//{
	//	const bool transmit_res = send_file(dir + ent->d_name, dir + ent->d_name,
	//		dst_sock, buf);

	//	if (!transmit_res)
	//		break;

	//	if (force_break)
	//		break;
	//}


	//for ()
	//{
	//	cerr << "File: '" << dir << ent->d_name << "'\n";
	//	const bool transmit_res = send_file(dir + ent->d_name, dir + ent->d_name,
	//		dst_sock, buf);
	//	free(ent);

	//	if (!transmit_res)
	//		break;
	//}




	//// We have to check if the sending buffer is empty.
	//// Or we will loose this data, because SRT is not waiting
	//// for all the data to be sent in a general live streaming use case,
	//// as it might be not something it is expected to do, and may lead to
	//// to unnesessary waits on destroy.
	//// srt_getsndbuffer() is designed to handle such cases.
	//const SRTSOCKET sock = m.Socket();
	//size_t blocks = 0;
	//do
	//{
	//	if (SRT_ERROR == srt_getsndbuffer(sock, &blocks, nullptr))
	//		break;

	//	if (blocks)
	//		this_thread::sleep_for(chrono::milliseconds(5));
	//} while (blocks != 0);

}


void xtransmit::file::send(const string& dst_url, const config& cfg, const atomic_bool& force_break)
{
	const vector<string> filenames = read_directory(cfg.src_path);

	if (filenames.empty())
	{
		cerr << "Found no files to transmit (path " << cfg.src_path << ")" << endl;
		return;
	}

	if (cfg.only_print)
	{
		cout << "Files found in " << cfg.src_path << endl;

		for_each(filenames.begin(), filenames.end(),
			[&dirpath = std::as_const(cfg.src_path)](const string& fname) {
				cout << fname << endl;
				cout << "RELATIVE: " << relative_path(fname, dirpath) << endl;
			});
		//copy(filenames.begin(), filenames.end(),
		//	ostream_iterator<string>(cout, "\n"));
		return;
	}

	UriParser ut(dst_url);
	ut["transtype"]  = string("file");
	ut["messageapi"] = string("true");
	ut["sndbuf"] = to_string(cfg.segment_size * 10);

	shared_srt_socket socket = make_shared<srt::socket>(ut);
	const bool        accept = socket->mode() == srt::socket::LISTENER;
	try
	{
		start_filesender(accept ? socket->async_accept() : socket->async_connect()
			, cfg, filenames, force_break);
	}
	catch (const srt::socket_exception & e)
	{
		cerr << e.what() << endl;
		return;
	}
}

#endif
