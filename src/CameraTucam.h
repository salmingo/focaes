/**
 * @class CameraTucam 鑫图相机控制接口
 * @date 2021-02-22
 * @version 0.1
 * @author Xiaomeng Lu
 */

#ifndef CAMERATUCAM_H_
#define CAMERATUCAM_H_

#include "TUDefine.h"
#include "TUCamApi.h"
#include "CameraBase.h"

class CameraTucam: public CameraBase {
public:
	CameraTucam();
	virtual ~CameraTucam();

protected:
	TUCAM_OPEN camOpen_;	/// 相机打开参数
	TUCAM_FRAME camFrm_;	/// 图像帧数据
	CAMERA_STATUS state_;	/// 相机工作状态, 指示曝光过程
	double expdur_;			/// 曝光时间
	threadptr thrd_waitfrm_;/// 线程: 等待读出图像
	boost::condition_variable cv_waitfrm_;	/// 条件: 曝光开始, 等待可以读出图像数据

protected:
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
	 * 工作状态
	 */
	CAMERA_STATUS CameraState();
	/*!
	 * @brief 继承类实现真正数据读出操作
	 * @return
	 * 工作状态
	 */
	CAMERA_STATUS DownloadImage();

protected:
	/*!
	 * @brief 线程: 等待曝光结束读出图像数据
	 */
	void thread_wait_frame();
};

#endif /* CAMERATUCAM_H_ */
