/*
 * @file ObservationSystem.cpp 关联调焦器网络连接和相机, 构成观测系统
 * @version 0.1
 * @date 2017-09-27
 */

#include "ObservationSystem.h"
#include "GLog.h"
#include "CameraApogee.h"
#include "CameraGY.h"

using namespace std;

obssptr make_obss(const string gid, const string uid, const string cid) {
	gLog.Write("Try to create Observation System <%s:%s:%s>", gid.c_str(), uid.c_str(), cid.c_str());
	obssptr obss = boost::make_shared<ObservationSystem>(gid, uid, cid);
	if (!obss->Start()) {
		gLog.Write(LOG_FAULT, "", "Failed to create Observation System <%s:%s:%s>",
				gid.c_str(), uid.c_str(), cid.c_str());
		obss.reset();
	}
	return obss;
}

ObservationSystem::ObservationSystem(const string gid, const string uid, const std::string cid) {
	group_id_ = gid;
	unit_id_  = uid;
	camera_id_= cid;
}

ObservationSystem::~ObservationSystem() {
}

bool ObservationSystem::Start() {
	return true;
}

void ObservationSystem::Stop() {

}

bool ObservationSystem::CameraOn(const std::string camip) {
	return true;
}

void ObservationSystem::CameraOff() {

}

void ObservationSystem::CoupleFocus(tcpcptr client) {
	if (client != tcpfoc_) {
		tcpfoc_ = client;
		gLog.Write("focuser<%s:%s:%s> is on-line", group_id_.c_str(),
				unit_id_.c_str(), camera_id_.c_str());
	}
}

void ObservationSystem::DecoupleFocus(tcpcptr client) {
	if (client == tcpfoc_) {
		tcpfoc_.reset();
		gLog.Write("focuser<%s:%s:%s> is off-line",
				group_id_.c_str(), unit_id_.c_str(), camera_id_.c_str());
	}
}

bool ObservationSystem::is_matched(const string gid, const string uid, const string cid) {

}
