/*
 * @file CameraGY.h GWAC定制相机(重庆港宇公司研发电控系统)控制接口声明文件
 * ============================================================================
 * @date May 09, 2017
 * @author Xiaomeng Lu
 * @version 0.1
 * @note
 * 环境配置(主要是GenICam SDK和Runtime)
 *
 * @note
 * - 由于读出等问题, 抛弃原基于JAISDK的x86平台, 重新基于GenICam开发接口, 封装为类CameraGY
 *
 * @note
 * 问题:
 * - socket采用阻塞式函数(send_to, receive_from), 当相机不可访问时不会收到相机反馈,
 *   程序卡死. 采用异步函数和延时判断(500ms)机制, 判定相机的可访问性
 * - 未实现增益与快门寄存器读取功能. 待实现
 * - 未实现0xF0F00830地址写入功能. 不再使用
 */

#ifndef CAMERAGY_H_
#define CAMERAGY_H_

#include "CameraBase.h"
#include "udp_asio.h"

//=============================================================================
/* 定义 */
#define PORT_CAMERA		3956		//< 相机UDP端口
#define PORT_LOCAL		49152		//< 本地UDP端口
/*!
 * @brief PACK_UDP_LEN UDP包最大有效数据长度
 *   1500 (UDP包总长度)
 * -   20 (IP包头长度)
 * -    8 (UDP包头长度)
 */
//#define PACK_UDP_LEN		1472
//#define PACK_HEAD_LEN		8		//< 数据包中信息头长度
/*!
 * @note 数据包类型标志
 */
#define ID_LEADER	0x01	// 引导数据包
#define ID_TRAILER	0x02	// 结尾数据包
#define ID_PAYLOAD	0x03	// 图像数据包

//=============================================================================
using boost::asio::ip::udp;

/* 港宇相机控制接口 */
class CameraGY: public CameraBase {
public:
	CameraGY(const std::string& camIP);
	virtual ~CameraGY();

public:
	/* 虚函数, 继承类实现 */
	/*!
	 * @brief 更改相机IP地址
	 * @param ip 新的IP地址
	 * @return
	 * 更改后IP地址
	 */
	const char *SetIP(const char *ip);
	/*!
	 * @brief 更改相机子网掩码
	 * @param mask 新的子网掩码
	 * @return
	 * 更改后子网掩码
	 */
	const char *SetNetmask(const char *mask);
	/*!
	 * @brief 更改相机网关
	 * @param gateway 新的网关
	 * @return
	 * 更改后网关
	 */
	const char *SetGateway(const char *gateway);

protected:
	/* 纯虚函数, 继承类实现 */
	/*!
	 * @brief 继承类实现与相机的真正连接
	 * @return
	 * 连接结果
	 */
	bool OpenCamera();
	/*!
	 * @brief 继承类实现真正与相机断开连接
	 */
	void CloseCamera();
	/*!
	 * @brief 设置制冷器工作模式及制冷温度
	 * @param coolerset  期望温度, 量纲: 摄氏度
	 * @param onoff      制冷器开关
	 */
	void CoolerOnOff(double& coolerset, bool& onoff);
	/*!
	 * @brief 设置读出端口
	 * @param index 读出端口档位
	 */
	void UpdateReadPort(uint32_t& index);
	/*!
	 * @brief 设置读出速度
	 * @param index 读出速度档位
	 */
	void UpdateReadRate(uint32_t& index);
	/*!
	 * @brief 设置增益
	 * @param index 增益档位
	 */
	void UpdateGain(uint32_t& index);
	/*!
	 * @brief 更新ROI区域
	 * @param xbin   X轴合并因子
	 * @param ybin   Y轴合并因子
	 * @param xstart X轴起始位置
	 * @param ystart Y轴起始位置
	 * @param width  宽度
	 * @param height 高度
	 */
	void UpdateROI(int& xbin, int& ybin, int& xstart, int& ystart, int& width, int& height);
	/*!
	 * @brief 自动调整偏置电压, 使得本底值尽可能接近offset
	 * @param offset 本底平均期望值
	 */
	void UpdateADCOffset(uint16_t offset);
	/*!
	 * @brief 查看相机芯片温度
	 * @return
	 * 相机芯片温度, 量纲: 摄氏度
	 */
	double SensorTemperature();
	/*!
	 * @brief 继承类实现启动真正曝光流程
	 * @param duration 曝光周期, 量纲: 秒
	 * @param light    是否需要外界光源
	 * @return
	 * 曝光启动结果
	 */
	bool StartExpose(double duration, bool light);
	/*!
	 * @brief 继承类实现真正中止当前曝光过程
	 */
	void StopExpose();
	/*!
	 * @brief 相机工作状态
	 * @return
	 * -1: 错误
	 *  0: 空闲
	 *  1: 曝光中
	 *  2: 曝光结束
	 */
	int CameraState();
	/*!
	 * @brief 继承类实现真正数据读出操作
	 */
	void DownloadImage();

private:
	/*!
	 * @brief 更改寄存器对应地址数值
	 * @param addr 地址
	 * @param val  数值
	 * @return
	 * 更改结果
	 */
	bool Write(uint32_t addr, uint32_t val);
	/*!
	 * @brief 查看寄存器对应地址数值
	 * @param addr 地址
	 * @param val  数值
	 * @return
	 * 查看结果
	 */
	bool Read(uint32_t addr, uint32_t &val);
	/*!
	 * @brief 检查并触发数据重传
	 */
	void Retransmit();
	/*!
	 * @brief 请求重传数据帧
	 * @param iPack0 重传帧起始编号
	 * @param iPack1 重传帧结束编号
	 * @return
	 * 重传指令处理结果
	 */
	bool Retransmit(uint32_t iPack0, uint32_t iPack1);
	/*!
	 * @brief 查看与相机IP在同一网段的本机IP地址
	 * @return
	 * 主机字节排序方式的本机地址
	 * @note
	 * 返回值0表示无效
	 */
	uint32_t GetHostAddr();
	/*!
	 * @brief 更新网络配置参数
	 * @param addr  地址
	 * @param vstr  新的网络地址/掩码/网关
	 * @return
	 * 更新后网络参数. 若更新失败则返回空字符串
	 */
	const char *UpdateNetwork(const uint32_t addr, const char *vstr);
	/*!
	 * @brief 心跳机制: 定时向相机发送心跳信号
	 */
	void ThreadHB();
	/*!
	 * @brief 当收到相机反馈信息时更新时间戳
	 * @param flag 时间戳
	 */
	void UpdateTimeFlag(int64_t &flag);
	/*!
	 * @brief 回调函数, 处理相机发送的UDP数据信息
	 * @param udp udp_session实例指针
	 * @param len 收到的数据长度, 量纲: 字节
	 */
	void ReceiveDataCB(const long udp, const long len);

private:
	/* 成员变量 */
	/* 相机工作参数 */
	std::string camIP_;		//< 相机IP地址
	uint32_t expdur_;		//< 曝光时间, 量纲: 微秒
	uint32_t shtrmode_;		//< 快门模式. 0: Normal; 1: AlwaysOpen; 2: AlwaysClose
	uint32_t gain_;			//< 增益. 0: 1x; 1: 2x; 2: 3x. x: e-/ADU
	int state_;				//< 工作状态
	bool aborted_;			//< 中止曝光标识
	/* 相关定义: 控制指令 */
	/*!
	 * - 通过UDP<IP_CAMERA, PORT_CAMERA>发送控制指令, 接收指令反馈
	 */
	udpptr udpcmd_;			//< 与相机间UDP指令接口
	uint16_t msgcnt_;		//< 指令帧序列编号
	boost::mutex mtxReg_;	//< 相机寄存器互斥锁
	threadptr thHB_;		//< 心跳线程
	/* 相关定义: 图像数据 */
	udpptr udpdata_;		//< 与相机间UDP数据接口
	int64_t tmdata_;		//< 数据时间戳
	int packtot_;			//< 图像数据包数量
	uint32_t byteimg_;		//< 图像数据大小, 量纲: 字节
	uint32_t bytercd_;		//< 已接收图像数据大小, 量纲: 字节
	/*!
	 * @brief bufpack_ 包缓存区
	 * 包缓存区单元大小为: packlen_, 由两部分组成
	 * - 起首4字节: 包有效数据大小, 量纲: 字节
	 * - 从第5个字节开始为有效数据
	 * - 第一个单元不使用
	 * @brief packlen_ 包缓存区空间
	 * Read(0x0D04, packlen_)获得网络支持的UDP包容量, 该值扣除20字节IP头+8字节UDP头+8字节相机定制头
	 */
	uint32_t packlen_;		//< 数据包大小
	uint32_t headlen_;		//< 定制数据头大小
	boost::shared_array<uint8_t> bufpack_;
	boost::shared_array<uint8_t> packflag_;	//< 包接收标记
	uint16_t idFrame_;	//< 图像帧编号
	uint32_t idPack_;	//< 一帧图像中的包编号
	boost::condition_variable imgrdy_;	//< 等待完成图像读出
	int nfail_;	//< 心跳连续错误计数
};

#endif /* CAMERAGY_H_ */
