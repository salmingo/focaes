/*
 * @file CameraApogee.cpp Apogee CCD相机控制接口
 * @version 0.2
 * @date Apr 9, 2017
 * @author Xiaomeng Lu
 */

#include <apogee/FindDeviceUsb.h>
#include <apogee/CameraInfo.h>
#include "apgSampleCmn.h"
#include "CameraApogee.h"

CameraApogee::CameraApogee() {
}

CameraApogee::~CameraApogee() {
}

bool CameraApogee::OpenCamera() {
	try {
		using std::string;
		string ioInterface("usb");
		FindDeviceUsb look4cam;
		string msg, addr;
		uint16_t id, frmwr;
		int count(0);	//< Apogee CCD连接后状态可能不对, 不能启动曝光. 后台建立尝试机制
		boost::chrono::seconds duration(1);

		altacam_ = boost::make_shared<Alta>();
		msg      = look4cam.Find();
		addr     = apgSampleCmn::GetUsbAddress(msg);
		id       = apgSampleCmn::GetID(msg);
		frmwr    = apgSampleCmn::GetFrmwrRev(msg);

		altacam_->OpenConnection(ioInterface, addr, frmwr, id);
		if (altacam_->IsConnected()) {
			// 相机初始状态为Status_Flushing时, 才可以正确启动曝光流程
			do {// 尝试多次初始化
				if (count) boost::this_thread::sleep_for(duration);
				altacam_->Init();
			} while(++count <= 10 && CameraState() > 0);

			if (CameraState()) {
				altacam_->CloseConnection();
				nfcam_->errmsg = "Wrong initial camera status";
			}
			else {
				nfcam_->model = altacam_->GetModel();
				nfcam_->wsensor = altacam_->GetMaxImgCols();
				nfcam_->hsensor = altacam_->GetMaxImgRows();
				nfcam_->roi.xstart = altacam_->GetRoiStartCol() + 1; // 起点调整为1
				nfcam_->roi.ystart = altacam_->GetRoiStartRow() + 1;
				nfcam_->roi.width  = altacam_->GetRoiNumCols();
				nfcam_->roi.height = altacam_->GetRoiNumRows();
				nfcam_->roi.xbin   = altacam_->GetRoiBinCol();
				nfcam_->roi.ybin   = altacam_->GetRoiBinRow();
				data.resize(nfcam_->roi.get_width() * nfcam_->roi.get_height());
			}
		}
		return altacam_->IsConnected();
	}
	catch(std::runtime_error &ex) {
		nfcam_->errmsg = ex.what();
		return false;
	}
	catch(...) {
		nfcam_->errmsg = "unknown error on connecting camera";
		return false;
	}
}

void CameraApogee::CloseCamera() {
	data.clear();
	altacam_->CloseConnection();
	altacam_.reset();
}

void CameraApogee::CoolerOnOff(double& coolerset, bool& onoff) {
	try {
		altacam_->SetCooler(onoff);
		if (onoff) altacam_->SetCoolerSetPoint(coolerset);
		coolerset = altacam_->GetCoolerSetPoint();
		onoff = altacam_->IsCoolerOn();
	}
	catch(std::runtime_error &ex) {
		nfcam_->errmsg = ex.what();
	}
}

void CameraApogee::UpdateReadPort(uint32_t& index) {
	//...
}

void CameraApogee::UpdateReadRate(uint32_t& index) {
	//...
}

void CameraApogee::UpdateGain(uint32_t& index) {
	//...
}

void CameraApogee::UpdateROI(int& xbin, int& ybin, int& xstart, int& ystart, int& width, int& height) {
	try {
		ROI& roi = nfcam_->roi;
		int n;

		if (xbin != roi.xbin) altacam_->SetRoiBinCol(xbin);
		if (ybin != roi.ybin) altacam_->SetRoiBinRow(ybin);
		if (xstart != roi.xstart) altacam_->SetRoiStartCol(xstart - 1);
		if (ystart != roi.ystart) altacam_->SetRoiStartRow(ystart - 1);
		if (width != roi.width)   altacam_->SetRoiNumCols(width);
		if (height != roi.height) altacam_->SetRoiNumRows(height);

		xbin = altacam_->GetRoiBinCol();
		ybin = altacam_->GetRoiBinRow();
		xstart = altacam_->GetRoiStartCol() + 1;
		ystart = altacam_->GetRoiStartRow() + 1;
		width  = altacam_->GetRoiNumCols();
		height = altacam_->GetRoiNumRows();
		if ((n = width * height) > data.capacity()) data.resize(n);
	}
	catch(std::runtime_error &ex) {
		nfcam_->errmsg = ex.what();
	}
}

void CameraApogee::UpdateADCOffset(uint16_t offset) {
	//...保留
}

double CameraApogee::SensorTemperature() {
	double val(0.0);
	try {
		val = altacam_->GetTempCcd();
	}
	catch(std::runtime_error &ex) {
		nfcam_->errmsg = ex.what();
	}
	return val;
}

bool CameraApogee::StartExpose(double duration, bool light) {
	try {
		altacam_->StartExposure(duration, light);
		return true;
	}
	catch(...) {
		nfcam_->errmsg = "unknown error during start new exposure";
		return false;
	}
}

void CameraApogee::StopExpose() {
	altacam_->StopExposure(true);
}

CAMERA_STATUS CameraApogee::CameraState() {
	Apg::Status status = altacam_->GetImagingStatus();
	CAMERA_STATUS retv;

	if (status == Apg::Status_ImageReady) retv = CAMERA_IMGRDY;
	else if (status > 0)  retv = CAMERA_EXPOSE;
	else if (status == 0) retv = CAMERA_IDLE;
	else retv = CAMERA_ERROR;
	return retv;
}

CAMERA_STATUS CameraApogee::DownloadImage() {
	try {
		int n = nfcam_->roi.get_width() * nfcam_->roi.get_height();
		altacam_->GetImage(data);
		memcpy(nfcam_->data.get(), (char*)&data[0], n * sizeof(unsigned short));
		return CAMERA_IMGRDY;
	}
	catch(...) {
		nfcam_->errmsg = "unknown error on readout";
		return CAMERA_ERROR;
	}
}
