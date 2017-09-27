/*!
 * @file udp_asio.h 基于boost::asio封装UDP通信
 * @version 0.1
 * @date May 23, 2017
 * @note
 */

#ifndef UDP_ASIO_H_
#define UDP_ASIO_H_

#include "ioservice_keep.h"
#include <boost/signals2.hpp>

#define UDP_BUFF_SIZE	1500

using boost::asio::ip::udp;

class udp_session {
public:
	/*!
	 * @brief 构造函数
	 * @param port  本机端口
	 * @param host  绑定UDP服务的本机IP. 若未指定则绑定所有IP
	 * @param who   身份. true: 服务器; false: 客户端
	 */
	udp_session(const int port = 0, const bool who = false);
	virtual ~udp_session();

public:
	typedef boost::signals2::signal<void (const long, const long)> cbfunc;	//< 回调函数
	typedef cbfunc::slot_type slottype;		//< 回调函数插槽

public:
	/*!
	 * @brief 设置远程主机
	 * @param peerIP     远程主机IP地址
	 * @param peerPort   远程主机UDP端口
	 * @return
	 * 若套接口上已建立连接则返回false， 否则返回true
	 * @note
	 * 后续通信发往远程主机
	 */
	void set_peer(const char *peerIP, const int peerPort);
	/*!
	 * @brief 关闭套接口
	 */
	void close();
	/*!
	 * @brief 检查套接口是否已经打开
	 * @return
	 * 套接口打开标识
	 */
	bool is_open();
	/*!
	 * @brief 读取已接收数据
	 * @param n 数据长度, 量纲: 字节
	 * @return
	 * 存储数据缓冲区地址
	 */
	const uint8_t *read(int &n);
	/*!
	 * @brief 延时读取已接收数据
	 * @param n 数据长度, 量纲: 字节
	 * @return
	 * 存储数据缓冲区地址
	 */
	const uint8_t *block_read(int &n);
	/*!
	 * @brief 将数据写入套接口
	 * @param data 待发送数据
	 * @param n    待发送数据长度, 量纲: 字节
	 * @return
	 * 操作结果
	 */
	void write(const void *data, const int n);

public:
	/*!
	 * @brief 注册异步接收回调函数
	 * @param slot 插槽
	 */
	void register_receive(const slottype &slot);
	/*!
	 * @brief 注册异步发送回调函数
	 * @param slot 插槽
	 */
	void register_send(const slottype &slot);

protected:
	/*!
	 * @brief 处理收到的网络信息
	 * @param ec 错误代码
	 * @param n  接收数据长度, 量纲: 字节
	 */
	void handle_receive(const boost::system::error_code& ec, const int n);
	/*!
	 * @brief 处理异步网络信息发送结果
	 * @param ec 错误代码
	 * @param n  发送数据长度, 量纲: 字节
	 */
	void handle_send(const boost::system::error_code& ec, const int n);
	/*!
	 * @brief 重新打开套接口
	 * @return
	 * 操作结果
	 */
	bool re_open();
	/*!
	 * @brief 异步接收信息
	 */
	void async_receive();

private:
	// 数据类型
	typedef boost::shared_ptr<udp::socket> sockptr;
	typedef boost::shared_ptr<boost::thread> threadptr;
	typedef boost::unique_lock<boost::mutex> mutex_lock;

	// 成员变量
	ioservice_keep keep_;	//< 维持boost::asio::io_service对象有效
	int locport_;			//< 本机端口
	std::string peerip_;	//< 当身份为客户端时, 记录服务器IP
	int peerport_;			//< 当身份为客户端时, 记录服务器端口
	sockptr sock_;			//< UDP套接口
	udp::endpoint epremote_;//< 对应远程端点地址
	bool whoami_;			//< 对应的网络身份. true: 服务器; false: 客户端
	cbfunc cbrcv_;			//< 接收回调函数
	cbfunc cbsnd_;			//< 发送回调函数
	int bytercv_;			//< 收到的字节数
	boost::shared_array<uint8_t> bufrcv_;	//< 接收缓存区

	boost::mutex mtxrcv_;	//< 接收互斥锁
	boost::mutex mtxsnd_;	//< 发送互斥锁
};
typedef boost::shared_ptr<udp_session> udpptr;

#endif
