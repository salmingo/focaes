/*
 * @file FileTransferClient.h 文件传输客户端
 * @date Apr 12, 2017
 * @version 0.1
 * @author Xiaomeng Lu
 */

#ifndef SRC_FILETRANSFERCLIENT_H_
#define SRC_FILETRANSFERCLIENT_H_

#include <list>
#include <string>
#include <string.h>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "ioservice_keep.h"

using namespace boost::posix_time;
using boost::asio::ip::tcp;
using std::string;

class FileTransferClient {
public:
	FileTransferClient();
	virtual ~FileTransferClient();

public:
	struct upload_file {// 待上传文件描述特着
		string grid_id;		//< 天区划分模式
		string field_id;	//< 天区编号
		string timeobs;		//< 起始曝光时间, 格式: CCYY-MM-DDThh:mm:ss.ssssss
		string filepath;	//< 文件本地全路径
		string subpath;		//< 子目录名
		string filename;	//< 文件名

	public:
		upload_file& operator=(const upload_file& other) {
			if (this != &other) {
				grid_id		= other.grid_id;
				field_id		= other.field_id;
				timeobs		= other.timeobs;
				filepath		= other.filepath;
				subpath		= other.subpath;
				filename		= other.filename;

				if (grid_id.empty()) grid_id = "undefined";
				if (field_id.empty()) field_id = "undefined";
			}

			return *this;
		}
	};

protected:
	/* 声明数据类型 */
	struct file_info {// 客户端->服务器
		char group_id[10];	//< 组标志
		char unit_id[10];	//< 单元标志
		char camera_id[10];	//< 相机标志
		char grid_id[10];	//< 天区划分模式
		char field_id[20];	//< 天区编号
		char timeobs[30];	//< 曝光起始时间
		char subpath[50];	//< 子目录名
		char filename[50];	//< 文件名
		int  filesize;		//< 文件大小, 量纲: 字节

	public:
		void fiel_info() {
			memset(this, 0, sizeof(file_info));
		}

		void set_devid(const std::string& gid, const std::string& uid, const std::string& cid) {
			strcpy(group_id,  gid.c_str());
			strcpy(unit_id,   uid.c_str());
			strcpy(camera_id, cid.c_str());
		}

		void set_file(const upload_file& upf) {
			strcpy(grid_id,  upf.grid_id.c_str());
			strcpy(field_id, upf.field_id.c_str());
			strcpy(timeobs,  upf.timeobs.c_str());
			strcpy(subpath,  upf.subpath.c_str());
			strcpy(filename, upf.filename.c_str());
		}
	};

	struct file_data {// 客户端->服务器
		int offset;			//< 偏移地址
		int size;			//< 包数据大小, 量纲: 字节
		char data[1440];	// 包数据
	};

	struct file_flag {// 服务器->客户端
		int flag; // 1: 收到文件头, 2: 文件接收完毕
	};

	typedef boost::shared_ptr<upload_file> upfptr;	//< 文件指针
	typedef std::list<upfptr> upflist;	//< 文件队列
	typedef boost::interprocess::message_queue msgque;
	typedef boost::shared_ptr<boost::thread> threadptr;	//< 线程指针
	typedef boost::mutex::scoped_lock mtxlck;
	typedef boost::mutex::scoped_try_lock mtxtlck;

	/* 成员变量 */
	std::string hostIP_;	//< 文件服务器IP地址
	int hostPort_;	//< 文件服务器服务端口
	upflist filelist_;		//< 待传送文件列表
	boost::mutex mtxsock_;		//< socket互斥锁
	boost::mutex mtxlist_;		//< 待上传文件列表互斥锁

	ioservice_keep keep_;	// asio::io_service服务
	boost::shared_ptr<tcp::socket> socket_;	//< 与文件服务器之间的网络连接
	ptime lastupd_;	//< 最后一次上传信息时间s

	std::string quename_;					//< 消息队列名称
	boost::shared_ptr<msgque> queue_;	//< 消息队列
	threadptr thrdUpd_;		//< 线程: 文件上传
	threadptr thrdAlive_;	//< 线程: 维护网络连接

	boost::shared_ptr<file_info> nffile_;	//< 待传输文件描述信息
	boost::shared_ptr<file_data> fdfile_;	//< 待传输文件数据
	boost::shared_ptr<file_flag> flagfile_;	//< 文件传输标记

public:
	/*!
	 * @brief 设置文件服务器
	 * @param ip   IP地址
	 * @param port 服务端口
	 */
	void SetHost(const std::string ip, const int port);
	/*!
	 * @brief 设置设备在网络中的标示
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @param cid 相机标志
	 */
	void SetDeviceID(const std::string& gid, const std::string& uid, const std::string& cid);
	/*!
	 * @brief 启动服务
	 */
	void Start();
	/*!
	 * @brief 中止服务
	 */
	void Stop();
	/*!
	 * @brief 声明需要传输的文件
	 * @param newfile  带传输文件描述信息
	 */
	void NewFile(upload_file* newfile);

protected:
	/*!
	 * @brief 连接服务器
	 * @return
	 * 与服务器的连接结果
	 */
	bool connect_server();
	/*!
	 * @brief 断开与服务器的连接
	 */
	void disconnect_server();
	/*!
	 * @brief 线程: 处理文件上传
	 */
	void ThreadUpload();
	/*!
	 * @brief 线程: 维护网络连接
	 * @note
	 * 功能:
	 * - 以1分钟为周期, 检查网络连接的有效性
	 * - 若未建立网络连接, 则尝试连接服务器
	 * - 若已建立网络连接, 则检查通道是否已经空闲超过1个周期. 若超过一个周期则发送KEEP_ALIVE信息
	 */
	void ThreadAlive();
	/*!
	 * @brief 上传队列中的
	 */
	void UploadFront();
	/*!
	 * @brief 上传一个文件
	 * @param file 文件信息
	 * @return
	 * 上传结果
	 */
	bool UploadFile(upfptr file);
	/*!
	 * @brief 触发上传新文件
	 * @param newfile 是否有新文件需要上传
	 */
	void TriggerUpload(bool newfile = true);
};

#endif /* SRC_FILETRANSFERCLIENT_H_ */
