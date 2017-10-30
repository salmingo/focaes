/*
 * @file CameraGY.cpp GWAC定制相机(重庆港宇公司研发电控系统)控制接口定义文件
 */

#include <boost/array.hpp>
#include <boost/make_shared.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <math.h>
#include <stdio.h>
#include "CameraGY.h"

//=============================================================================
CameraGY::CameraGY(const std::string& camIP) {
	// 相机控制与工作参数
	camIP_ 		= camIP;
	expdur_		= UINT_MAX;
	shtrmode_	= UINT_MAX;
	gain_		= UINT_MAX;
	msgcnt_		= 0;
	tmdata_		= -100000;
	state_		= -1;
	aborted_	= false;
	// 数据缓冲区
	byteimg_	= 0;
	bytercd_	= 0;
	packtot_	= -1;
	// 初始化数据传输接口
	const udp_session::slottype &slot1 = boost::bind(&CameraGY::ReceiveDataCB, this, _1, _2);
	udpdata_ = boost::make_shared<udp_session>(PORT_LOCAL, true);
	udpdata_->register_receive(slot1);
	// 初始化指令传输接口
	udpcmd_ = boost::make_shared<udp_session>();
	udpcmd_->set_peer(camIP.c_str(), PORT_CAMERA);
}

CameraGY::~CameraGY() {
}

// 连接相机
bool CameraGY::OpenCamera() {
	try {
		uint32_t addrHost = GetHostAddr();
		if (addrHost == 0x00) throw std::runtime_error("no matched host IP address");

		using boost::asio::ip::address_v4;
		boost::array<uint8_t, 8> buff1 = {0x42, 0x01, 0x00, 0x02, 0x00, 0x00};
		int bytercv, trycnt(0);
		address_v4 addr1, addr2;
		const uint8_t *buff2;

		msgcnt_ = 0;
		do {
			((uint16_t*)&buff1)[3] = htons(++msgcnt_);
			udpcmd_->write(buff1.c_array(), buff1.size());
			buff2 = udpcmd_->block_read(bytercv);
		} while (bytercv < 48 && ++trycnt < 3);
		if (bytercv < 48) throw std::runtime_error("failed to communicate with camera");
		addr1 = address_v4(buff2[47] + uint32_t(buff2[46] << 8) + uint32_t(buff2[45] << 16) + uint32_t(buff2[44] << 24));
		addr2 = address_v4::from_string(camIP_);
		if (addr1 != addr2) throw std::runtime_error("not found camera");

		/* 初始化参数 */
		Write(0x0A00,     0x03);		// Set GevCCP
		Write(0x0D00,     PORT_LOCAL);	// Set GevSCPHostPort
		Write(0x0D04,     1500);		// Set PacketSize
		Write(0x0D08,     0);			// Set PacketDelay
		Write(0x0D18,     addrHost);	// Set GevSCDA
		Write(0xA000,     0x01);		// Start AcquisitionSequence
		// 初始化监测量
		uint32_t val;
		state_ = 0;
		Read(0xA004, val); nfcam_->wsensor = int(val);
		Read(0xA008, val); nfcam_->hsensor = int(val);
		byteimg_ = nfcam_->wsensor * nfcam_->hsensor * 2;
		Read(0x0D04, packlen_);
		headlen_ = 8;
		packlen_ -= (20 + 8 + headlen_); // 20: IP Header; 8: UDP Header; headlen_: Customized Header
		packtot_ = int(ceil(double(byteimg_ + 64) / packlen_)); // 最后一包多出64字节
		packlen_ += 4; // 4: buffer header
		packflag_.reset(new uint8_t[packtot_ + 1]);
		bufpack_.reset(new uint8_t[(packtot_ + 1) * packlen_]);
		Read(0x00020008, gain_);
		Read(0x0002000C, shtrmode_);
		Read(0x00020010, expdur_);
		// 启动心跳机制, 维护与相机间的网络连接
		nfail_ = 0;
		thHB_.reset(new boost::thread(boost::bind(&CameraGY::ThreadHB, this)));

		return true;
	}
	catch(std::exception &ex) {
		nfcam_->errmsg = ex.what();
		return false;
	}
}

// 断开相机连接
void CameraGY::CloseCamera() {
	if (nfcam_->exposing && state_ >= 0) {// 错误时, StopExpose()无法中止曝光
		boost::chrono::milliseconds t(50);
		StopExpose();
		while (nfcam_->exposing && state_ >= 0) boost::this_thread::sleep_for(t);
	}
	ExitThread(thHB_);
	udpdata_->close();
	udpcmd_->close();
}

// 设置制冷器工作模式及制冷温度
void CameraGY::CoolerOnOff(double& coolerset, bool& onoff) {
	//...
}

// 设置读出端口
void CameraGY::UpdateReadPort(uint32_t& index) {
//...不支持该功能
}

// 设置读出速度
void CameraGY::UpdateReadRate(uint32_t& index) {
//...不支持该功能
}

// 设置增益
void CameraGY::UpdateGain(uint32_t& index) {
	if (0 <= index && index <= 2 && index != gain_) {
		try {
			Write(0x00020008, index);
			Read(0x00020008, index);
		}
		catch(std::runtime_error& ex) {
			nfcam_->errmsg = ex.what();
		}
	}
}

// 更新ROI区域
void CameraGY::UpdateROI(int& xbin, int& ybin,
		int& xstart, int& ystart, int& width, int& height) {
//...不支持该功能
}

void CameraGY::UpdateADCOffset(uint16_t offset) {
	//...不支持该功能
}

// 查看相机芯片温度
double CameraGY::SensorTemperature() {
	//...不支持该功能
	return -30.0;
}

// 启动真正曝光流程
bool CameraGY::StartExpose(double duration, bool light) {
	try {
		// 设置监测量
		if (aborted_) aborted_ = false;
		bytercd_ = 0;
		idFrame_  = uint16_t(-1);
		idPack_   = uint32_t(-1);
		memset(packflag_.get(), 0, packtot_ + 1);
		// 设置曝光参数
		uint32_t val;
		if (shtrmode_ != (val = light ? 0 : 2) && Write(0x0002000C, val)) {
			if (!Read(0x0002000C, shtrmode_))
				throw std::runtime_error("failed to read shutter-mode");
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
		}
		if (expdur_ != (val = uint32_t(duration * 1E6)) && Write(0x00020010, val)) {
			if (!Read(0x00020010, expdur_))
				throw std::runtime_error("failed to read exposure-duration");
		}
		Write(0x00020000, 0x01);
		state_ = 1;

		return true;
	}
	catch(std::runtime_error &ex) {
		nfcam_->errmsg = ex.what();
		return false;
	}
}

// 中止当前曝光过程
void CameraGY::StopExpose() {
	try {
		Write(0x20050, 0x1);
		aborted_ = true;
//		state_ = 0;	// 中断且抛弃已累积数据
	}
	catch(std::runtime_error& ex) {
		nfcam_->errmsg = ex.what();
		state_ = -1;
	}
}

// 相机工作状态
int CameraGY::CameraState() {
	return state_;
}

// 数据读出操作
void CameraGY::DownloadImage() {
	boost::mutex tmp;
	mutex_lock lck(tmp);
	imgrdy_.wait(lck); // 等待图像就绪标志
	if (!aborted_ && bytercd_ == byteimg_) {// 合并数据
		uint8_t *data = nfcam_->data.get();
		uint8_t *buff = bufpack_.get() + packlen_;
		uint32_t len;
		for (int i = 1; i <= packtot_ && packflag_[i]; ++i, buff += packlen_) {
			len = ((uint32_t*) buff)[0];
			memcpy(data, buff + 4, len);
			data += len;
		}
	}
	state_ = (aborted_ || bytercd_ < byteimg_) ? -1 : 0;
}

bool CameraGY::Write(uint32_t addr, uint32_t val) {
	mutex_lock lck(mtxReg_);
	boost::array<uint8_t, 16> buff1 = {0x42, 0x01, 0x00, 0x82, 0x00, 0x08};
	int n;

	((uint16_t*) &buff1)[3] = htons(++msgcnt_);
	((uint32_t*) &buff1)[2] = htonl(addr);
	((uint32_t*) &buff1)[3] = htonl(val);
	udpcmd_->write(buff1.c_array(), buff1.size());
	const uint8_t *buff2 = udpcmd_->block_read(n);

	// 2017-10-21: 输出收到的信息
	if (n != 12 || buff2[11] != 0x01) {
		char txt[200];
		int n1 = sprintf(txt, "length<%d> of write register<%0X>: ", n, addr);
		for (int i = 0; i < n; ++i) n1 += sprintf(txt + n1, "%02X ", buff2[i]);
		throw std::runtime_error(txt);
	}
	return buff2[11] == 0x01;
}

bool CameraGY::Read(uint32_t addr, uint32_t &val) {
	mutex_lock lck(mtxReg_);
	boost::array<uint8_t, 12> buff1 = {0x42, 0x01, 0x00, 0x80, 0x00, 0x04};
	int n;

	((uint16_t*) &buff1)[3] = htons(++msgcnt_);
	((uint32_t*) &buff1)[2] = htonl(addr);
	udpcmd_->write(buff1.c_array(), buff1.size());
	const uint8_t *buff2 = udpcmd_->block_read(n);
	if (n != 12) {
		// 2017-10-21: 输出收到的信息
		char txt[200];
		int n1 = sprintf(txt, "length<%d> of read register<%0X>: ", n, addr);
		for (int i = 0; i < n; ++i) n1 += sprintf(txt + n1, "%02X ", buff2[i]);
		throw std::runtime_error(txt);
	}
	else val = ntohl(((uint32_t*)buff2)[2]);
	return n == 12;
}

void CameraGY::Retransmit() {
	int i, pck0, pck1;
	int n = packtot_;

	for (i = 1; i <= n && packflag_[i]; ++i);
	if (i > n) {
		/* 所有包都已接收, 但数据量不对, 数据包接收不完整, 网络状态异常
		 * - 终止读出过程
		 * - 终止程序, 需要用户干预检查网络状态或相机状态
		 */
//		if (!aborted_) imgrdy_.notify_one();
//		else state_ = -1;
		imgrdy_.notify_one();
	}
	else {
		pck0 = i;
		for (; i <= n && !packflag_[i]; ++i);
		pck1 = i - 1;
		Retransmit(pck0, pck1);
	}
}

bool CameraGY::Retransmit(uint32_t iPack0, uint32_t iPack1) {
	mutex_lock lck(mtxReg_);
	boost::array<uint8_t, 20> buff = {0x42, 0x00, 0x00, 0x40, 0x00, 0x0c};
	((uint16_t*)&buff)[3] = htons(++msgcnt_);
	((uint32_t*)&buff)[2] = htonl(idFrame_);
	((uint32_t*)&buff)[3] = htonl(iPack0);
	((uint32_t*)&buff)[4] = htonl(iPack1);
	udpcmd_->write(buff.c_array(), buff.size());

	return true;
}

uint32_t CameraGY::GetHostAddr() {
	ifaddrs *ifaddr, *ifa;
	uint32_t addr(0), addrcam, mask;
	bool found(false);

	inet_pton(AF_INET, camIP_.c_str(), &addrcam);
	if (!getifaddrs(&ifaddr)) {
		for (ifa = ifaddr; ifa != NULL && !found; ifa = ifa->ifa_next) {// 只采用IPv4地址
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
			addr = ((struct sockaddr_in*) ifa->ifa_addr)->sin_addr.s_addr;
			mask = ((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr.s_addr;
			found = (addr & mask) == (addrcam & mask); // 与相机IP在同一网段
		}
		freeifaddrs(ifaddr);
	}

	return found ? ntohl(addr) : 0;
}

const char *CameraGY::SetIP(const char *ip) {
	return UpdateNetwork(0x064C, ip);
}

const char *CameraGY::SetNetmask(const char *mask) {
	return UpdateNetwork(0x065C, mask);
}

const char *CameraGY::SetGateway(const char *gateway) {
	return UpdateNetwork(0x066C, gateway);
}

const char *CameraGY::UpdateNetwork(const uint32_t addr, const char *vstr) {
	if (!nfcam_->connected) return NULL;

	try {
		uint32_t value;
		static char buff[INET_ADDRSTRLEN];
		inet_pton(AF_INET, vstr, &value);
		if (Write(addr, ntohl(value)) && Read(addr, value)) {
			value = htonl(value);
			return inet_ntop(AF_INET, &addr, buff, INET_ADDRSTRLEN);
		}
	}
	catch(std::runtime_error& ex) {
		nfcam_->errmsg = ex.what();
	}
	return NULL;
}

void CameraGY::ThreadHB() {
	boost::chrono::milliseconds p(1000);	// 闲(非曝光)态心跳周期: 1秒c
	ptime now;
	ptime::time_duration_type td;
	int busy_t1(p.count() * 3), busy_t2(busy_t1 - 86400000);	// 忙(曝光)态无反馈阈值
	int dt;
	bool brun(true);

	while(brun) {
		boost::this_thread::sleep_for(p);
		try {
			Write(0x0938, 0x2EE0);
			if (nfail_) nfail_ = 0;
		}
		catch(std::runtime_error& ex) {
			++nfail_;
			nfcam_->errmsg = ex.what();
		}

		if (nfail_ < 5 && nfcam_->exposing) {
			now = microsec_clock::universal_time();
			td = now - nfcam_->tmobs;

			if ((td.total_seconds() - nfcam_->eduration) > 10.0) {
				StopExpose();
				imgrdy_.notify_one();
			}
			else if (idFrame_ != 0xFFFF && bytercd_ < byteimg_) {
				dt = now.time_of_day().total_milliseconds() - tmdata_;
				if (dt > busy_t1 || dt < busy_t2) Retransmit();
			}
		}
		else if (nfail_ >= 5) {
			//...
		}
	}
}

void CameraGY::UpdateTimeFlag(int64_t &flag) {
	flag = microsec_clock::universal_time().time_of_day().total_milliseconds();
}

void CameraGY::ReceiveDataCB(const long udp, const long len) {
	UpdateTimeFlag(tmdata_);
	if (bytercd_ == byteimg_) return; // 所有数据都已接收, 抛弃trailer

	if (state_ == 1 && !aborted_) state_ = 2; // 相机状态置为曝光结束, 主程序设置时标(曝光结束)
	int n;
	const uint8_t *pack = udpdata_->read(n);

	uint16_t status  = (pack[0] << 8) | pack[1];
	uint16_t idFrame = (pack[2] << 8) | pack[3];	// 图像帧编号
	uint8_t  type    = pack[4]; // 数据包类型
	uint32_t idPack  = (pack[5] << 16) | (pack[6] << 8) | pack[7]; // 包编号
	if (type == ID_LEADER) {// idPack_==0
		idFrame_ = idFrame;
		idPack_  = idPack;
	}
	else if (type == ID_PAYLOAD) {
		// 处理收到的数据包
		uint32_t packlen = len - headlen_;
		uint8_t *ptr = bufpack_.get() + idPack * packlen_;
		if (idPack == packtot_) packlen -= 64;
		((uint32_t*) ptr)[0] = packlen;
		memcpy(ptr + 4, pack + headlen_, packlen);
		packflag_[idPack] = 1;
		bytercd_ += packlen;
		packflag_[idPack] = 1;
		// 检测数据是否完整接收
		if (bytercd_ == byteimg_) imgrdy_.notify_one();
		else {// 数据接收完毕, 合并数据
			if (idFrame_ == 0xFFFF) idFrame_ = idFrame;// 未收到leader
			else if (idPack != (idPack_ + 1)) Retransmit(); // 立即重传?
			idPack_ = idPack;
		}
	}
	else if (type == ID_TRAILER) {
		if (aborted_) {// 中止时收到包尾, 通知完成数据接收
			imgrdy_.notify_one();
		}
		else {// 数据不全但收到trailer, 中间必然丢包, 申请重传
			idPack_  = idPack;
			Retransmit();
		}
	}
}
