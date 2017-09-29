/*
 * @file CameraBase.cpp 相机基类定义文件, 各型号相机共同访问控制接口
 * @date 2017年4月9日
 * @version 0.2
 * @author Xiaomeng Lu
 */

#include <boost/make_shared.hpp>
#include "CameraBase.h"

using namespace std;

CameraBase::CameraBase() {
	nfcam_ = boost::make_shared<devcam_info>();
}

CameraBase::~CameraBase() {
	Disconnect();
	nfcam_.reset();
}

boost::shared_ptr<devcam_info> CameraBase::GetCameraInfo() {
	return nfcam_;
}

void CameraBase::register_expose(const ExposeProcess::slot_type& slot) {
	exposeproc_.connect(slot);
}

bool CameraBase::IsConnected() {
	return nfcam_->connected;
}

bool CameraBase::Connect() {
	if (nfcam_->connected) return true;
	if (!OpenCamera()) return false;

	int n;
	nfcam_->connected = true;
	nfcam_->roi.reset(nfcam_->wsensor, nfcam_->hsensor);
	n = nfcam_->roi.get_width() * nfcam_->roi.get_height();
	n = (n * 2 + 15) & ~15;	// 长度对准16字节
	nfcam_->data.reset(new uint8_t[n]);
	// 线程
	thrdIdle_.reset(new boost::thread(boost::bind(&CameraBase::ThreadIdle, this)));
	thrdExpose_.reset(new boost::thread(boost::bind(&CameraBase::ThreadExpose, this)));

	return true;
}

void CameraBase::Disconnect() {
	if (!nfcam_->connected) return;
	ExitThread(thrdIdle_);
	ExitThread(thrdExpose_);

	SetCooler(0.0, false);
	CloseCamera();
	nfcam_->connected = false;
}

void CameraBase::SetCooler(double coolerset, bool onoff) {
	if (!nfcam_->connected) return ;

	CoolerOnOff(coolerset, onoff);
	nfcam_->coolerset = coolerset;
	nfcam_->cooling   = onoff;
}

void CameraBase::SetReadPort(uint32_t index) {
	if (!nfcam_->connected || nfcam_->exposing) return ;

	UpdateReadPort(index);
	nfcam_->readport = index;
}

void CameraBase::SetReadRate(uint32_t index) {
	if (!nfcam_->connected || nfcam_->exposing) return ;

	UpdateReadRate(index);
	nfcam_->readrate = index;
}

void CameraBase::SetGain(uint32_t index) {
	if (!nfcam_->connected || nfcam_->exposing) return ;

	UpdateGain(index);
	nfcam_->gain = index;
}

void CameraBase::SetROI(int xbin, int ybin, int xstart, int ystart, int width, int height) {
	if (!nfcam_->connected || nfcam_->exposing) return ;

	/* 参数有效性初步判断 */
	int res;
	if (xbin <= 0) xbin = 1;
	if (ybin <= 0) ybin = 1;
	if (xstart <= 0 || xstart >= nfcam_->wsensor) xstart = 1;
	if (ystart <= 0 || ystart >= nfcam_->hsensor) ystart = 1;
	if ((res = xstart % xbin) > 0) xstart -= res;
	if ((res = ystart % ybin) > 0) ystart -= res;
	if (width <= 0 || width > nfcam_->wsensor)   width = nfcam_->wsensor;
	if (height <= 0 || height > nfcam_->hsensor) height = nfcam_->hsensor;
	if ((res = xstart + width - 1 - nfcam_->wsensor) > 0)  width -= res;
	if ((res = ystart + height - 1 - nfcam_->hsensor) > 0) height -= res;
	if ((res = width % xbin) > 0)  width -= res;
	if ((res = height % ybin) > 0) height -= res;
	if (width <= 0) {
		xstart = 1;
		width = nfcam_->wsensor - nfcam_->wsensor % xbin;
	}
	if (height <= 0) {
		ystart = 1;
		height = nfcam_->hsensor - nfcam_->hsensor % ybin;
	}

	/* 更新ROI区 */
	ROI& roi = nfcam_->roi;
	int oldn = roi.get_width() * roi.get_height();
	int newn;

	UpdateROI(xbin, ybin, xstart, ystart, width, height);
	if (roi.xbin != xbin)     roi.xbin = xbin;
	if (roi.ybin != ybin)     roi.ybin = ybin;
	if (roi.xstart != xstart) roi.xstart = xstart;
	if (roi.ystart != ystart) roi.ystart = ystart;
	if (roi.width != width)   roi.width = width;
	if (roi.height != height) roi.height = height;
	newn = (roi.get_width() * roi.get_height() * 2 + 15) & ~15;
	if (oldn != newn) nfcam_->data.reset(new uint8_t[newn]);
}

void CameraBase::SetADCOffset(uint16_t offset) {
	if (!nfcam_->connected || nfcam_->exposing) return;
	nfcam_->exposing = true;
	UpdateADCOffset(offset);
	nfcam_->exposing = false;
}

bool CameraBase::Expose(double duration, bool light) {
	if (!nfcam_->connected || nfcam_->exposing) return false;
	if (!StartExpose(duration, light)) return false;

	nfcam_->begin_expose(duration);
	condexp_.notify_one();
	nfcam_->format_utc();
	nfcam_->check_ampm();

	return true;
}

void CameraBase::AbortExpose() {
	if (!nfcam_->exposing) return;
	StopExpose();
}

void CameraBase::ThreadIdle() {
	boost::chrono::seconds duration(5);

	while(1) {
		boost::this_thread::sleep_for(duration);
		if (!nfcam_->exposing) nfcam_->coolerget = SensorTemperature();
	}
}

void CameraBase::ThreadExpose() {
	boost::mutex mtx;    // 哑元
	mutex_lock lck(mtx);
	boost::chrono::milliseconds duration;	// 等待周期
	double left, percent;
	int status, ms;

	while (true) {
		condexp_.wait(lck);
		while ((status = CameraState()) == 1) {// 监测曝光过程
			nfcam_->check_expose(left, percent);
			if (left > 0.1) ms = 100;
			else ms = int(left * 1000);
			duration = boost::chrono::milliseconds(ms);

			exposeproc_(left, percent, 2);
			if (ms > 0) boost::this_thread::sleep_for(duration);
		}
		if (status == 2) {
			nfcam_->end_expose();
			exposeproc_(0.0, 100.0, 2);
			DownloadImage();
		}
		nfcam_->exposing = false;
		exposeproc_(0.0, 100.001, status == 2 ? 1 : -1);
	}
}

void CameraBase::ExitThread(threadptr &thrd) {
	if (thrd.unique()) {
		thrd->interrupt();
		thrd->join();
		thrd.reset();
	}
}

const char *CameraBase::SetIP(const char *ip) {
	return NULL;
}

const char *CameraBase::SetNetmask(const char *mask) {
	return NULL;
}

const char *CameraBase::SetGateway(const char *gateway) {
	return NULL;
}
