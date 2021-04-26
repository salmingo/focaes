/**
 * @class CameraTucam 鑫图相机控制接口
 * @date 2021-02-22
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * - 厂商文档中, 关于可控制参数的说明严重缺失
 */
#include <string.h>
#include "CameraTucam.h"

CameraTucam::CameraTucam() {
	state_ = CAMERA_IDLE;
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
	frame_.pBuffer = NULL;
	frame_.ucFormat = TUFRM_FMT_RAW;
	frame_.uiRsdSize = 1;
	if (TUCAM_Buf_Alloc(camOpen_.hIdxTUCam, &frame_) != TUCAMRET_SUCCESS) {
		nfcam_->errmsg = "TUCAM_Buf_Alloc() error";
		return false;
	}

	state_ = CAMERA_IDLE;
	return true;
}

void CameraTucam::CloseCamera() {
	if (camOpen_.hIdxTUCam) {
		if (state_ > CAMERA_IDLE) TUCAM_Cap_Stop(camOpen_.hIdxTUCam);
		TUCAM_Buf_Release(camOpen_.hIdxTUCam);
		TUCAM_Dev_Close(camOpen_.hIdxTUCam);
	}
	TUCAM_Api_Uninit();
}

void CameraTucam::CoolerOnOff(double& coolerset, bool& onoff) {
	// 来源: SDK使用说明, p.67
	// 温度设置阈值: 0-100, 对应实际温度: -50~+50
	// 没有独立停止制冷功能
	double val = (onoff ? coolerset : 0.0) + 50.0;
	TUCAM_Prop_SetValue(camOpen_.hIdxTUCam, TUIDP_TEMPERATURE, val);
}

void CameraTucam::UpdateReadPort(uint32_t& index) {

}

void CameraTucam::UpdateReadRate(uint32_t& index) {

}

void CameraTucam::UpdateGain(uint32_t& index) {

}

void CameraTucam::UpdateROI(int& xbin, int& ybin, int& xstart, int& ystart, int& width, int& height) {

}

void CameraTucam::UpdateADCOffset(uint16_t offset) {

}

double CameraTucam::SensorTemperature() {
	double val;
	TUCAM_Prop_GetValue(camOpen_.hIdxTUCam, TUIDP_TEMPERATURE, &val);
	return val;
}

bool CameraTucam::StartExpose(double duration, bool light) {
	bool ret = TUCAM_Prop_SetValue(camOpen_.hIdxTUCam, TUIDP_EXPOSURETM, duration) == TUCAMRET_SUCCESS
			&& TUCAM_Cap_DoSoftwareTrigger(camOpen_.hIdxTUCam) == TUCAMRET_SUCCESS;
	if (ret) state_ = CAMERA_EXPOSE;
	return ret;
}

void CameraTucam::StopExpose() {
	if (TUCAM_Cap_Stop(camOpen_.hIdxTUCam) == TUCAMRET_SUCCESS)
		state_ = CAMERA_IDLE;
}

CAMERA_STATUS CameraTucam::CameraState() {
	if (nfcam_->percent >= 100.0) state_ = CAMERA_IMGRDY;
	return CAMERA_IDLE;
}

CAMERA_STATUS CameraTucam::DownloadImage() {
	int ret;
//	TUCAM_Buf_WaitForFrame();
	state_ = CAMERA_IDLE;
	return ret == TUCAMRET_SUCCESS ? CAMERA_IDLE : CAMERA_ERROR;
}
