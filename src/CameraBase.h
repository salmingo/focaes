/*
 * @file CameraBase.h 相机基类声明文件, 各型号相机共同访问控制接口
 * @date 2017年4月9日
 * @version 0.2
 * @author Xiaomeng Lu
 * @note
 * 功能列表:
 * @li 实现相机工作逻辑流程
 * @li 声明真实相机需实现的纯虚函数
 */

#ifndef CAMERABASE_H_
#define CAMERABASE_H_

#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <boost/signals2.hpp>

using namespace boost::posix_time;

enum CAMERA_STATUS {// 相机工作状态
	CAMERA_ERROR,	// 错误
	CAMERA_IDLE,	// 空闲
	CAMERA_EXPOSE,	// 曝光过程中
	CAMERA_IMGRDY,	// 已完成曝光, 可以读出数据进入内存
	CAMERA_LAST		// 占位
};

enum CAMCTL_STATUS {// 相机控制状态
	CAMCTL_ERROR,		// 错误
	CAMCTL_IDLE,		// 空闲
	CAMCTL_EXPOSE,		// 曝光过程中
	CAMCTL_COMPLETE,	// 已完成曝光
	CAMCTL_ABORT,		// 已中止曝光
	CAMCTL_PAUSE,		// 已暂停曝光
	CAMCTL_WAIT_TIME,	// 等待曝光流传起始时间到达
	CAMCTL_WAIT_FLAT,	// 平场间等待--等待转台重新指向
	CAMCTL_LAST			// 占位
};

struct ROI {// ROI区, XY坐标起始点为(1,1)
	int xstart, ystart;		// ROI区左上角在全幅中的位置
	int width, height;		// ROI区在全副中的宽度和高度
	int xbin, ybin;			// ROI区X、Y方向合并因子

public:
	int get_width() {// 合并后图像宽度
		return width / xbin;
	}

	int get_height() {// 合并后图像高度
		return height / ybin;
	}

	void reset(int w, int h) {// 重置ROI区
		xstart = ystart = 1;
		xbin = ybin = 1;
		width  = w;
		height = h;
	}
};

struct devcam_info {// 相机设备基本信息
	std::string errmsg;		//< 错误提示
	std::string model;		//< 相机型号
	bool connected;			//< 与相机连接标志
	bool cooling;			//< 制冷启动标志
//	bool exposing;			//< 曝光过程标志
	CAMERA_STATUS state;	//< 工作状态
	int errorno;			//< 错误代码
	int wsensor;			//< 图像宽度, 量纲: 像素
	int hsensor;			//< 图像高度, 量纲: 像素
	uint32_t readport;		//< 读出端口档位
	uint32_t readrate;		//< 读出速度档位
	uint32_t gain;			//< 增益档位
	double coolerset;		//< 制冷温度
	double coolerget;		//< 芯片温度
	ROI roi;				//< 有效ROI区
	double eduration;		//< 曝光时间, 量纲: 秒
	std::string dateobs;	//< 曝光起始时间对应的日期, 格式: CCYY-MM-DD
	std::string timeobs;	//< 曝光起始时间对应的时间, 格式: hh:mm:ss.ssssss
	std::string timeend;	//< 曝光结束时间对应的时间, 格式: hh:mm:ss.ssssss
	ptime tmobs;			//< 曝光起始时间, 用于监测曝光进度
	double jd;				//< 曝光起始时间对应的儒略日
	std::string utcdate;	//< 曝光起始日期, 用于生成文件存储目录名, 格式: YYMMDD
	std::string utctime;	//< 曝光起始时间, 用于生成文件名, 格式: YYMMDDThhmmssss
	bool   ampm;			//< true: A.M.; false: P.M.
	boost::shared_array<uint8_t> data;	//< 图像数据存储区

public:
	virtual ~devcam_info() {
		data.reset();
	}

	void begin_expose(double duration) {
		tmobs = microsec_clock::universal_time();
		eduration = duration;
		state     = CAMERA_EXPOSE;
	}

	void end_expose() {
		ptime now = microsec_clock::universal_time();
		timeend = to_simple_string(now.time_of_day());
	}

	/*!
	 * @brief 构建用于目录名和文件名的字符串
	 */
	void format_utc() {// 构建字符串带来的延时约为0.02毫秒
		ptime::date_type date = tmobs.date();
		boost::format fmtdate = boost::format("%02d%02d%02d")
				% (date.year() - 2000)
				% (date.month().as_number())
				% (date.day());
		utcdate = fmtdate.str();
		dateobs = to_iso_extended_string(date);

		ptime::time_duration_type time = tmobs.time_of_day();
		boost::format fmttime = boost::format("%sT%02d%02d%04d")
				% utcdate.c_str()
				% (time.hours())
				% (time.minutes())
				% (time.seconds() * 100 + time.fractional_seconds() / 10000);
		utctime = fmttime.str();
		timeobs = to_simple_string(time);

		double fd = time.total_microseconds() * 1E-6 / 86400.0;
		jd = date.julian_day() + fd - 0.5;
	}

	void check_ampm() {
		ptime now(microsec_clock::local_time());
		ampm = now.time_of_day().hours() < 12;
	}

	/*!
	 * @brief 检查曝光进度
	 * @param left    剩余时间, 量纲: 秒
	 * @param percent 曝光进度, 百分比
	 * @return
	 * 曝光未完成返回true, 否则返回false
	 */
	bool check_expose(double& left, double& percent) {
		ptime now(microsec_clock::universal_time());
		boost::posix_time::time_duration elps = now - tmobs;
		double dt = elps.total_microseconds() * 1E-6;
		if ((left = eduration - dt) < 0.0 || eduration < 1E-6) {
			left = 0.0;
			percent = 100.000001;
		}
		else percent = dt * 100.000001 / eduration;
		return percent <= 100.0;
	}
};
/*!
 * @brief 声明曝光进度回调函数
 * @param <1> 曝光剩余时间, 量纲: 秒
 * @param <2> 曝光进度, 量纲: 百分比
 * @param <3> 图像数据状态, CAMERA_STATUS
 */
typedef boost::signals2::signal<void (const double, const double, const int)> ExposeProcess;

class CameraBase {
public:
	CameraBase();
	virtual ~CameraBase();

protected:
	/* 声明数据类型 */
	typedef boost::shared_ptr<boost::thread> threadptr;
	typedef boost::unique_lock<boost::mutex> mutex_lock;

	/* 声明成员变量 */
	boost::shared_ptr<devcam_info> nfcam_;	//< 相机基本信息
	boost::condition_variable condexp_;		//< 通知曝光开始
	ExposeProcess exposeproc_;				//< 曝光进度回调函数
	threadptr thrdIdle_;	//< 线程: 空闲时监测温度
	threadptr thrdExpose_;	//< 线程: 监测曝光进度和结果

public:
	/*!
	 * @brief 注册曝光进度回调函数
	 * @param slot 插槽函数
	 */
	void register_expose(const ExposeProcess::slot_type& slot);

public:
	boost::shared_ptr<devcam_info> GetCameraInfo();
	/*!
	 * @brief 相机连接标志
	 * @return
	 * 是否已经建立与相机连接标志
	 */
	bool IsConnected();
	/*!
	 * @brief 尝试连接相机
	 * @return
	 * 相机连接结果
	 */
	bool Connect();
	/*!
	 * @brief 断开与相机的连接
	 */
	void Disconnect();
	/*!
	 * @brief 设置制冷器工作模式及制冷温度
	 * @param coolerset  期望温度, 量纲: 摄氏度
	 * @param onoff      制冷器开关
	 */
	void SetCooler(double coolerset = -20.0, bool onoff = false);
	/*!
	 * @brief 设置读出端口
	 * @param index 读出端口档位
	 */
	void SetReadPort(uint32_t index);
	/*!
	 * @brief 设置读出速度
	 * @param index 读出速度档位
	 */
	void SetReadRate(uint32_t index);
	/*!
	 * @brief 设置增益
	 * @param index 增益档位
	 */
	void SetGain(uint32_t index);
	/*!
	 * @brief 设置ROI区域
	 * @param xbin   X轴合并因子
	 * @param ybin   Y轴合并因子
	 * @param xstart X轴起始位置
	 * @param ystart Y轴起始位置
	 * @param width  宽度
	 * @param height 高度
	 */
	void SetROI(int xbin = 1, int ybin = 1, int xstart = 1, int ystart = 1, int width = -1, int height = -1);
	/*!
	 * @brief 设置本底基准值
	 * @param offset 基准值
	 */
	void SetADCOffset(uint16_t offset);
	/*!
	 * @brief 尝试启动曝光流程
	 * @param duration  曝光周期, 量纲: 秒
	 * @param light     是否需要外界光源
	 * @return
	 * 曝光启动结果
	 */
	bool Expose(double duration, bool light = true);
	/*!
	 * @brief 中止当前曝光过程
	 */
	void AbortExpose();

protected:
	/*!
	 * @brief 空闲线程, 监测相机温度
	 */
	void ThreadIdle();
	/*!
	 * @brief 曝光线程, 监测曝光进度和结果
	 */
	void ThreadExpose();
	/*!
	 * @brief 统一线程结束操作
	 * @param thrd 线程接口
	 */
	void ExitThread(threadptr &thrd);

public:
	/* 虚函数, 继承类实现 */
	/*!
	 * @brief 更改相机IP地址
	 * @param ip 新的IP地址
	 * @return
	 * 更改后IP地址
	 */
	virtual const char *SetIP(const char *ip);
	/*!
	 * @brief 更改相机子网掩码
	 * @param mask 新的子网掩码
	 * @return
	 * 更改后子网掩码
	 */
	virtual const char *SetNetmask(const char *mask);
	/*!
	 * @brief 更改相机网关
	 * @param gateway 新的网关
	 * @return
	 * 更改后网关
	 */
	virtual const char *SetGateway(const char *gateway);

protected:
	/* 纯虚函数, 继承类实现 */
	/*!
	 * @brief 继承类实现与相机的真正连接
	 * @return
	 * 连接结果
	 */
	virtual bool OpenCamera() = 0;
	/*!
	 * @brief 继承类实现真正与相机断开连接
	 */
	virtual void CloseCamera() = 0;
	/*!
	 * @brief 设置制冷器工作模式及制冷温度
	 * @param coolerset  期望温度, 量纲: 摄氏度
	 * @param onoff      制冷器开关
	 */
	virtual void CoolerOnOff(double& coolerset, bool& onoff) = 0;
	/*!
	 * @brief 设置读出端口
	 * @param index 读出端口档位
	 */
	virtual void UpdateReadPort(uint32_t& index) = 0;
	/*!
	 * @brief 设置读出速度
	 * @param index 读出速度档位
	 */
	virtual void UpdateReadRate(uint32_t& index) = 0;
	/*!
	 * @brief 设置增益
	 * @param index 增益档位
	 */
	virtual void UpdateGain(uint32_t& index) = 0;
	/*!
	 * @brief 更新ROI区域
	 * @param xbin   X轴合并因子
	 * @param ybin   Y轴合并因子
	 * @param xstart X轴起始位置
	 * @param ystart Y轴起始位置
	 * @param width  宽度
	 * @param height 高度
	 */
	virtual void UpdateROI(int& xbin, int& ybin, int& xstart, int& ystart, int& width, int& height) = 0;
	/*!
	 * @brief 自动调整偏置电压, 使得本底值尽可能接近offset
	 * @param offset 本底平均期望值
	 */
	virtual void UpdateADCOffset(uint16_t offset) = 0;
	/*!
	 * @brief 查看相机芯片温度
	 * @return
	 * 相机芯片温度, 量纲: 摄氏度
	 */
	virtual double SensorTemperature() = 0;
	/*!
	 * @brief 继承类实现启动真正曝光流程
	 * @param duration 曝光周期, 量纲: 秒
	 * @param light    是否需要外界光源
	 * @return
	 * 曝光启动结果
	 */
	virtual bool StartExpose(double duration, bool light) = 0;
	/*!
	 * @brief 继承类实现真正中止当前曝光过程
	 */
	virtual void StopExpose() = 0;
	/*!
	 * @brief 相机工作状态
	 * @return
	 * 工作状态
	 */
	virtual CAMERA_STATUS CameraState() = 0;
	/*!
	 * @brief 继承类实现真正数据读出操作
	 * @return
	 * 工作状态
	 * @note
	 * 返回值对应DownloadImage的结果, 选项为:
	 * CAMERA_IMGRDY: 读出成功
	 * CAMERA_IDLE:   读出失败, 源于中止曝光
	 * CAMERA_ERROR:  相机错误
	 */
	virtual CAMERA_STATUS DownloadImage() = 0;
};

#endif /* CAMERABASE_H_ */
