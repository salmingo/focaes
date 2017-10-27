/*
 * @fle FileTransferClient.cpp 文件传输客户端
 * @date Apr 12, 2017
 * @version 0.1
 * @author Xiaomeng Lu
 */

#include <boost/make_shared.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "FileTransferClient.h"
#include "GLog.h"

using namespace std;
using namespace boost::interprocess;

FileTransferClient::FileTransferClient() {
	hostIP_   = "";
	hostPort_ = 0;
	nffile_ = boost::make_shared<file_info>();
	fdfile_ = boost::make_shared<file_data>();
	flagfile_ = boost::make_shared<file_flag>();
}

FileTransferClient::~FileTransferClient() {
	Stop();
}

void FileTransferClient::SetHost(const string ip, const int port) {
	hostIP_   = ip;
	hostPort_ = port;
}

void FileTransferClient::Start() {
	quename_ = "mq_camagent_ftcli";
	msgque::remove(quename_.c_str());
	queue_.reset(new msgque(boost::interprocess::create_only, quename_.c_str(), 1024, sizeof(char)));
	thrdUpd_.reset(new boost::thread(boost::bind(&FileTransferClient::ThreadUpload, this)));
	thrdAlive_.reset(new boost::thread(boost::bind(&FileTransferClient::ThreadAlive, this)));
}

void FileTransferClient::Stop() {
	TriggerUpload(false);
	if (thrdUpd_.unique()) {
		thrdUpd_->join();
		thrdUpd_.reset();
	}
	if (queue_.unique()) {
		msgque::remove(quename_.c_str());
		queue_.reset();
	}
	if (socket_.unique() && socket_->is_open()) {
		socket_->close();
		socket_.reset();
	}
	if (filelist_.size()) {
		gLog.Write("%d files left un-upload", filelist_.size());
		filelist_.clear();
	}
}

void FileTransferClient::SetDeviceID(const string& gid, const string& uid, const string& cid) {
	nffile_->set_devid(gid, uid, cid);
}

void FileTransferClient::NewFile(upload_file* newfile) {
	mtxlck lock(mtxlist_);
	upfptr file = boost::make_shared<upload_file>();
	*file = *newfile;
	filelist_.push_back(file);
	TriggerUpload();
}

bool FileTransferClient::connect_server() {
	if (!socket_.unique()) {
		try {
			tcp::resolver resolver(keep_.get_service());
			tcp::resolver::query query(hostIP_, boost::lexical_cast<string>(hostPort_));
			tcp::resolver::iterator itertor = resolver.resolve(query);
			socket_.reset(new tcp::socket(keep_.get_service()));
			boost::asio::connect(*socket_, itertor);
			lastupd_ = second_clock::universal_time();
		}
		catch(exception& ex) {
			gLog.Write("Failed to connect File Server<%s:%d>: %s", hostIP_.c_str(), hostPort_, ex.what());
			socket_.reset();
		}
	}

	return (socket_.unique() && socket_->is_open());
}

void FileTransferClient::disconnect_server() {
	if (socket_.unique() && socket_->is_open()) {
		socket_->close();
		socket_.reset();
	}
}

void FileTransferClient::ThreadUpload() {
	char msg;
	msgque::size_type recvd_size;
	msgque::size_type msg_size = sizeof(char);
	unsigned int priority;

	do {
		queue_->receive((void*) &msg, msg_size, recvd_size, priority);
		if (msg) UploadFront();
	} while(msg);
}

void FileTransferClient::ThreadAlive() {
	boost::chrono::minutes duration(1);
	double limit = duration.count() * 60.0;
	ptime::time_duration_type dt;
	ptime now;

	connect_server();
	while(1) {
		boost::this_thread::sleep_for(duration);

		if (!socket_.unique()) {
			if (connect_server()) TriggerUpload(); // 重联服务器后, 触发一次文件上传
		}
		else {
			dt = (now = second_clock::universal_time()) - lastupd_;
			if (limit <= dt.total_milliseconds() * 0.001) {
				mtxtlck tlock(mtxsock_); // 判断是否正在传输文件
				if (tlock.owns_lock()) {
					nffile_->filesize = 0;
					try {
						socket_->write_some(boost::asio::buffer(nffile_.get(), sizeof(file_info)));
					}
					catch(exception& ex) {
						gLog.Write(LOG_WARN, "FileTransferClient", ex.what());
						socket_.reset();
					}
				}
			}
		}
	}
}

void FileTransferClient::UploadFront() {
	mtxtlck tlock(mtxsock_); // 判断是否正在传输文件
	if (!tlock.owns_lock()) return ; // 套接口占用: 正在上传数据
	if (!socket_.unique())  return ; // 与服务器连接断开

	upfptr file;
	if (filelist_.size()) {
		mtxlck lock(mtxlist_);
		file = filelist_.front();
	}

	if (file.use_count() && UploadFile(file)) {
		// 结束上传
		mtxlck lock(mtxlist_);
		filelist_.pop_front();
		if (filelist_.size()) TriggerUpload(); // 有新文件需要上传
	}
}

bool FileTransferClient::UploadFile(upfptr file) {
	try {
		gLog.Write("Upload file <%s>", file->filename.c_str());
		lastupd_ = second_clock::universal_time();

		file_mapping my_file(file->filepath.c_str(), read_only);
		mapped_region region(my_file, read_only);
		char* headptr = (char*) region.get_address();
		int filesize  = region.get_size();
		int pack_size = 1440;
		int offset(0);

		nffile_->set_file(*file);
		nffile_->filesize = filesize;

		socket_->write_some(boost::asio::buffer(nffile_.get(), sizeof(file_info)));
		socket_->read_some(boost::asio::buffer(flagfile_.get(), sizeof(file_flag)));
		while (filesize > 0) {
			if (pack_size > filesize) pack_size = filesize;
			fdfile_->offset = offset;
			fdfile_->size   = pack_size;
			memcpy(fdfile_->data, headptr, pack_size);
			socket_->write_some(boost::asio::buffer(fdfile_.get(), sizeof(file_data)));
			headptr += pack_size;
			offset  += pack_size;
			filesize-= pack_size;
		}
		socket_->read_some(boost::asio::buffer(flagfile_.get(), sizeof(file_flag)));
		gLog.Write("Upload over");

		return true;
	}
	catch(exception& ex) {
		gLog.Write(LOG_FAULT, "UploadFile()", ex.what());
		socket_.reset();
		return false;
	}
}

void FileTransferClient::TriggerUpload(bool newfile) {
	if (!queue_.unique()) return;

	char msg(1);
	int priority(1);
	if (!newfile) {
		msg = 0;
		priority = 10;
	}
	queue_->send(&msg, sizeof(char), priority);
}
