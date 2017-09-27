/*
 * @file ObservationSystem.h 关联调焦器网络连接和相机, 构成观测系统
 * @version 0.1
 * @date 2017-09-27
 * - 关联调焦器网络连接和相机(GWAC-GY和U9000, 默认索引5对应U9000)
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include "msgque_base.h"
#include "CameraBase.h"
#include "tcp_asio.h"

class ObservationSystem: public msgque_base {
public:
	ObservationSystem(const std::string gid, const std::string uid, const std::string cid);
	virtual ~ObservationSystem();

private:
	/* 数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock;	// 互斥锁

public:
	/*!
	 * @brief 启动观测系统服务
	 * @return
	 * 服务启动结果
	 */
	bool Start();
	/*!
	 * @brief 停止观测系统服务
	 */
	void Stop();
	/*!
	 * @brief 启动相机
	 * @param camip 相机IP. 当cid==005时忽略IP地址
	 * @return
	 * 相机启动结果
	 */
	bool CameraOn(const std::string camip = "");
	/*!
	 * @brief 停用相机
	 */
	void CameraOff();
	/*!
	 * @brief 关联观测系统和调焦器
	 * @param client 网络连接
	 */
	void CoupleFocus(tcpcptr client);
	/*!
	 * @brief 解除观测系统和调焦器的关联关系
	 * @param client 网络连接
	 */
	void DecoupleFocus(tcpcptr client);
	/*!
	 * @brief 匹配检查观测系统
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @param cid 相机标志
	 * @return
	 * 匹配结果
	 */
	bool is_matched(const std::string gid, const std::string uid, const std::string cid);

private:
	/* 成员变量 */
	std::string group_id_;		//< 组标志
	std::string unit_id_;		//< 单元标志
	std::string camera_id_;		//< 相机标志
	tcpcptr tcpfoc_;			//< 对应调焦器的网络连接
};
typedef boost::shared_ptr<ObservationSystem> obssptr;
/*!
 * @brief 工厂函数, 构建观测系统
 * @param gid 组标志
 * @param uid 单元标志
 * @param cid 相机标志或相机IP地址
 * @return
 * 观测系统访问指针
 */
extern obssptr make_obss(const std::string gid, const std::string uid, const std::string cid);

#endif /* OBSERVATIONSYSTEM_H_ */
