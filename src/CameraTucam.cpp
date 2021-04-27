/**
 * @class CameraTucam 鑫图相机控制接口
 * @date 2021-02-22
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * - 厂商文档中, 关于可控制参数的说明严重缺失
 */
#include <boost/bind/bind.hpp>
#include <string.h>
#include "CameraTucam.h"

using namespace boost::placeholders;

CameraTucam::CameraTucam() {
	state_ = CAMERA_IDLE;
	expdur_= 0.0;
}

CameraTucam::~CameraTucam() {

}

bool CameraTucam::OpenCamera() {
	TUCAM_INIT itprm;
	itprm.pstrConfigPath = "/usr/local/etc"; // 相机参数路径

	if (TUCAM_Api_Init(&itprm) != TUCAMRET_SUCCESS
			|| itprm.uiCamCount == 0) {
		nfcam_->errmsg = "TUCAM_Api_Init() error or not found camera";
		return false;
	}

	camOpen_.uiIdxOpen = 0;
	if (TUCAM_Dev_Open(&camOpen_) != TUCAMRET_SUCCESS) {
		nfcam_->errmsg = "TUCAM_Dev_Open() error";
		return false;
	}

	TUCAM_VALUE_INFO value;
	value.nID = TUIDI_CAMERA_MODEL;
	TUCAM_Dev_GetInfo(camOpen_.hIdxTUCam, &value);
	nfcam_->model = value.pText;

	// 分配图像存储空间并获取靶面分辨率
	camFrm_.pBuffer = NULL;
	camFrm_.ucFormatGet = TUFRM_FMT_RAW;
	camFrm_.uiRsdSize = 1;
	if (TUCAM_Buf_Alloc(camOpen_.hIdxTUCam, &camFrm_) != TUCAMRET_SUCCESS) {
		nfcam_->errmsg = "TUCAM_Buf_Alloc() error";
		return false;
	}
	nfcam_->wsensor = camFrm_.usWidth;
	nfcam_->hsensor = camFrm_.usHeight;
	expdur_= -1E30;
	TUCAM_Cap_Start(camOpen_.hIdxTUCam, TUCCM_TRIGGER_SOFTWARE);
	thrd_waitfrm_.reset(new boost::thread(boost::bind(&CameraTucam::thread_wait_frame, this)));

	state_ = CAMERA_IDLE;
	return true;
}

void CameraTucam::CloseCamera() {
	if (camOpen_.hIdxTUCam) {
		if (state_ > CAMERA_IDLE) {
			TUCAM_Buf_AbortWait(camOpen_.hIdxTUCam);
			// 等待
			boost::chrono::milliseconds d(50);
			while (state_ > CAMERA_IDLE)
				boost::this_thread::sleep_for(d);
		}
		TUCAM_Cap_Stop(camOpen_.hIdxTUCam);
		TUCAM_Buf_Release(camOpen_.hIdxTUCam);
		TUCAM_Dev_Close(camOpen_.hIdxTUCam);
	}
	TUCAM_Api_Uninit();
	// 销毁线程
	if (thrd_waitfrm_.unique()) {
		thrd_waitfrm_->interrupt();
		thrd_waitfrm_->join();
	}
}

void CameraTucam::CoolerOnOff(double& coolerset, bool& onoff) {
	// 来源: SDK使用说明, p.67
	// 温度设置阈值: 0-100, 对应实际温度: -50~+50
	// 没有独立停止制冷功能
	double val = (onoff ? coolerset : 0.0) + 50.0;
	TUCAM_Prop_SetValue(camOpen_.hIdxTUCam, TUIDP_TEMPERATURE, val);
}

void CameraTucam::UpdateReadPort(uint32_t& index) {
	//...无效
}

void CameraTucam::UpdateReadRate(uint32_t& index) {
	//...无效
}

void CameraTucam::UpdateGain(uint32_t& index) {
	double gain;
	TUCAM_Prop_SetValue(camOpen_.hIdxTUCam, TUIDP_GLOBALGAIN, index);
	TUCAM_Prop_GetValue(camOpen_.hIdxTUCam, TUIDP_GLOBALGAIN, &gain);
	index = uint32_t(gain + 0.5);
}

void CameraTucam::UpdateROI(int& xbin, int& ybin, int& xstart, int& ystart, int& width, int& height) {
	if (state_ == CAMERA_IDLE) {

	}
}

void CameraTucam::UpdateADCOffset(uint16_t offset) {
	//...
}

double CameraTucam::SensorTemperature() {
	double val;
	TUCAM_Prop_GetValue(camOpen_.hIdxTUCam, TUIDP_TEMPERATURE, &val);
	return val;
}

bool CameraTucam::StartExpose(double duration, bool light) {
	if (duration != expdur_
			&& TUCAM_Prop_SetValue(camOpen_.hIdxTUCam, TUIDP_EXPOSURETM, duration * 1000) == TUCAMRET_SUCCESS) {
		expdur_ = duration;
	}
	if (TUCAM_Cap_DoSoftwareTrigger(camOpen_.hIdxTUCam) == TUCAMRET_SUCCESS) {
		state_ = CAMERA_EXPOSE;
		cv_waitfrm_.notify_one();
		return true;
	}
	return false;
}

void CameraTucam::StopExpose() {
	if (state_ > CAMERA_IDLE
			&& TUCAM_Buf_AbortWait(camOpen_.hIdxTUCam) == TUCAMRET_SUCCESS) {
		boost::chrono::milliseconds d(10);
		while (state_ > CAMERA_IDLE)
			boost::this_thread::sleep_for(d);
	}
}

CAMERA_STATUS CameraTucam::CameraState() {
	return state_;
}

CAMERA_STATUS CameraTucam::DownloadImage() {
	int n = nfcam_->roi.get_width() * nfcam_->roi.get_height();
	memcpy(nfcam_->data.get(), camFrm_.pBuffer + camFrm_.usHeader, n * sizeof(unsigned short));
	state_ = CAMERA_IDLE;
	return CAMERA_IMGRDY;
}

void CameraTucam::thread_wait_frame() {
	boost::mutex mtx;    // 哑元
	mutex_lock lck(mtx);
	int code;

	while (true) {
		cv_waitfrm_.wait(lck); // 等待曝光起始信号
		if ((code = TUCAM_Buf_WaitForFrame(camOpen_.hIdxTUCam, &camFrm_)) == TUCAMRET_SUCCESS) {
			TUCAM_Buf_CopyFrame(camOpen_.hIdxTUCam, &camFrm_);
			state_ = CAMERA_IMGRDY;
		}
		else {
			state_ = code == TUCAMRET_ABORT ? CAMERA_IDLE : CAMERA_ERROR;
		}
	}
}
