/*
 Name        : focaes.cpp
 Author      : Xiaomeng Lu
 Copyright   : NAOC
 Description : 焦平面调节辅助工具
 设计说明:
 Date:         2017-09-21
 Version     : 0.1
 */

#include <boost/interprocess/ipc/message_queue.hpp>
#include <longnam.h>
#include <fitsio.h>
#include <xpa.h>
#include "globaldef.h"
#include "GLog.h"
#include "gui.h"
#include "termscreen.h"
#include "parameter.h"
#include "tcp_asio.h"
#include "mountproto.h"
#include "CameraBase.h"
#include "CameraApogee.h"
#include "CameraGY.h"
#include "FileTransferClient.h"

//////////////////////////////////////////////////////////////////////////////
#define VALID_FOCUS 10000

/// 数据结构定义
enum IMAGE_TYPE {// 曝光类型
	IMGTYPE_BIAS = 1,	// 本底
	IMGTYPE_DARK,	// 暗场
	IMGTYPE_FLAT,	// 平场
	IMGTYPE_OBJECT,	// 目标
	IMGTYPE_FOCUS,	// 调焦
	IMGTYPE_LAST	// 占位, 无效类型
};

enum MODE {// 工作模式
	MODE_INIT = 1,	// 初始化
	MODE_MANUAL,	// 手动
	MODE_AUTO,		// 自动
	MODE_CAL		// 定标: 修正偏置电压
};

struct systate {// 系统工作状态
	MODE mode;				//< 工作模式
	std::string cid;		//< 相机标志
	std::string objname;	//< 目标名称
	IMAGE_TYPE imgtype;		//< 图像类型
	std::string imgtypestr;	//< 图像类型字符串型
	std::string imgtypeabbr;//< 图像类型字符串型缩略
	std::string termtype;	//< 终端类型
	double expdur;			//< 曝光时间
	int frmcnt;				//< 曝光总帧数
	int frmno;				//< 曝光序号
	std::string pathname;	//< 目录名
	std::string filename;	//< 文件名
	std::string filepath;	//< 文件全路径

public:
	systate() {
		mode = MODE_INIT;
		cid  = "";
		imgtype = IMGTYPE_LAST;
		expdur  = 5.0;
		frmcnt  = -1;
		frmno   = -1;
	}

	void reset() {
		mode = MODE_INIT;
		cid  = "";
		imgtype = IMGTYPE_LAST;
		expdur  = 5.0;
		frmcnt  = -1;
		frmno   = -1;
	}

	void set_exposure(IMAGE_TYPE type, int count = -1, double duration = -1.0, std::string name = "") {
		if (imgtype != type) imgtype = type;
		if (count > 0) frmcnt = count;
		if (type != IMGTYPE_BIAS && duration > 1E-6) expdur = duration;
		if (type == IMGTYPE_BIAS) {
			objname = "bias";
			imgtypestr = "BIAS";
			imgtypeabbr= "bias";
			expdur = 0.0;
		}
		else if (type == IMGTYPE_DARK) {
			objname = "dark";
			imgtypestr = "DARK";
			imgtypeabbr= "dark";
		}
		else {
			objname = name;
			imgtypestr = "OBJECT";
			imgtypeabbr= "objt";
		}
		if (frmcnt <= 0) frmcnt = 1;
		if (expdur < 0.0) expdur = 2.0;
		frmno = 0;
	}
};

struct focuser {// 调焦器
	tcpcptr tcp;	//< 网络连接
	int posAct;	//< 实际位置
	int posTar;	//< 目标位置
	int repeat;	//< 由静至动再至静的重复次数

public:
	void reset() {
		posAct = posTar = VALID_FOCUS;
		repeat = 0;
	}
};

typedef boost::interprocess::message_queue msgque;
typedef boost::unique_lock<boost::mutex> mutex_lock;

//////////////////////////////////////////////////////////////////////////////
/// 全局变量
GLog gLog;
GLog gLog1(stdout);
param_config param;						//< 配置参数
boost::shared_ptr<msgque> queue;			//< 消息队列
boost::shared_ptr<boost::thread> thrdmsg;	//< 消息队列线程句柄
systate state;							//< 系统状态
boost::shared_ptr<tcp_server> tcpsfoc;	//< 调焦服务器
focuser focus;							//< 调焦器
boost::shared_ptr<CameraBase> camera;	//< 相机控制接口
mntptr mntproto;	//< 通信协议接口
int curpos;		//< 光标位置
boost::mutex mtxcur;	//< 光标互斥区
static char flags[] = "|/-\\";
static int iflag(-1);
bool firstimg(true);
boost::shared_ptr<FileTransferClient> ftcli;	// 文件上传接口

//////////////////////////////////////////////////////////////////////////////
/// 全局函数
void SendMessage(const long msg); // 投递高优先级消息
void PostMessage(const long msg); // 投递低优先级消息
/*==========================================================================*/
/// 界面交互
/*!
 * @brief 显示使用帮助
 */
void PrintHelp() {
	int x(1), y(LINE_SEPARATOR - 1);
	std::string seps = std::string(86, '*');

	ShowCursor(false);
	PrintXY(x, ++y, "\033[92;49m%s\033[0m", seps.c_str());
	PrintXY(x, ++y, "* On <Camera_IP>            # connect camera. empty for U9000.       keyword: \033[93;49m\033[1mon\033[0m     *");
	PrintXY(x, ++y, "* Off                       # disconnect camera.                     keyword: \033[93;49m\033[1moff\033[0m    *");
	PrintXY(x, ++y, "* Bias <count>              # take sequential BIAS image.            keyword: \033[93;49m\033[1mB\033[0mias   *");
	PrintXY(x, ++y, "* Dark <duration> <count>   # take sequential DARK image.            keyword: \033[93;49m\033[1mD\033[0mark   *");
	PrintXY(x, ++y, "* name <duration> <count>   # take sequential LIGHT image                            *");
	PrintXY(x, ++y, "* Focus <position>          # change focuser position.               keyword: \033[93;49m\033[1mF\033[0mocus  *");
	PrintXY(x, ++y, "* Reload                    # reload configuration file.             keyword: \033[93;49m\033[1mR\033[0meload *");
	PrintXY(x, ++y, "* Start                     # start sequence based on configuration. keyword: \033[93;49m\033[1mstart\033[0m  *");
	PrintXY(x, ++y, "* Stop                      # stop running sequence.                 keyword: \033[93;49m\033[1mstop\033[0m   *");
	PrintXY(x, ++y, "* Quit                      # quit program.                          keyword: \033[93;49m\033[1mQ\033[0muit   *");
	PrintXY(x, ++y, "\033[92;49m%s\033[0m", seps.c_str());
}

void PrintServer() {// 显示调焦器网络服务端口
	ShowCursor(false);
	PrintXY(1, LINE_PORT, "Focus Daemon Port: %d", param.portFocus);
}

void PrintInput() {// 显示用户输入提示符
	mutex_lock lck(mtxcur);
	curpos = 3;
	PrintXY(1, LINE_INPUT, "\033[93;49m\033[1m>\033[0m\033[0m ");
	ShowCursor(true);
	UpdateScreen();
}

void PrintAutoParameter() {// 显示自动控制参数
	ShowCursor(false);
	PrintXY(1, LINE_PARAM, "group=%s, unit=%s, expdur=%.3f, count=%d, focuser<from %d to %d, step=%d, error=%d>",
			param.grpid.c_str(), param.unitid.c_str(),
			param.expdur, param.frmcnt,
			param.stroke_start, param.stroke_stop, param.stroke_step, param.focuser_error);
}

void PrintManualParameter() {// 显示手动控制参数
	ShowCursor(false);
	PrintXY(1, LINE_PARAM, "ImageType=%s, name=%s, expdur=%.3f, frmcnt=%d",
			state.imgtype == IMGTYPE_BIAS ? "BIAS" : (state.imgtype == IMGTYPE_DARK ? "DARK" : "OBJECT"),
					state.objname.c_str(), state.expdur, state.frmcnt);
}

void PrintFocus() {// 显示焦点位置
	int act(focus.posAct), tar(focus.posTar);
	if ((act != VALID_FOCUS) || (tar != VALID_FOCUS)) {
		char buff[100];
		int n;
		n = sprintf(buff, "focuser:");
		if (act != VALID_FOCUS) n += sprintf(buff + n, "  position=%d", act);
		if (tar != VALID_FOCUS) sprintf(buff + n, "  target=%d", tar);

		ShowCursor(false);
		PrintXY(1, LINE_FOCUS, "%s", buff);
		mutex_lock lck(mtxcur);
		MovetoXY(curpos, LINE_INPUT);
		ShowCursor(true);
		UpdateScreen();
	}
}

void ClearError() {// 清除错误信息
	PrintXY(1, LINE_ERROR, "");
}

/*!
 * @brief 显示曝光进度
 * @param process 曝光进度, 量纲: 百分比
 */
void PrintExprocess(double process) {
	if (process > 100.0) return;
	process *= 0.01;

	int N1, N2, N, n(0);    // n1(/), n2(\)
	char txt[100];
	txt[64] = ' ';
	N = process >= 0.999 ? 86 : 85;
	N1 = (int) (N * process);
	N2 = (int) (N * (1.0 - process));
	if (N1 > 0) n = sprintf(txt, "%s", std::string(N1, '/').c_str());
	if (N2 > 0) n += sprintf(txt + n, "%s", std::string(N2, '\\').c_str());
	if (N == 85) {
		iflag = ++iflag % 4;
		txt[n++] = flags[iflag];
	}
	sprintf(txt + 85, " %5.1f%%", process * 100.000001);

	ShowCursor(false);
	PrintXY(1, LINE_EXPROCESS, "%s", txt);
	mutex_lock lck(mtxcur);
	MovetoXY(curpos, LINE_INPUT);
	ShowCursor(true);
	UpdateScreen();
}
/*==========================================================================*/
/* 显示FITS图像 */
bool XPASetFile(const char *filepath, const char *filename, bool back) {
	char *names, *messages;
	char params[100];
	char mode[50];

	strcpy(mode, back ? "ack=true" : "ack=false");
	sprintf(params, "fits %s", filename);
	int fd = open(filepath, O_RDONLY);
	if (fd >= 0) {
		XPASetFd(NULL, (char*) "ds9", params, mode, fd, &names, &messages, 1);
		if (names) free(names);
		if (messages) free(messages);
		close(fd);
		return true;
	}
	return false;
}

void XPASetScaleMode(const char *mode) {
	char *names, *messages;
	char params[40];

	sprintf(params, "scale mode %s", mode);
	XPASet(NULL, (char*) "ds9", params, NULL, NULL, 0, &names, &messages, 1);
	if (names) free(names);
	if (messages) free(messages);
}

void XPASetZoom(const char *zoom) {
	char *names, *messages;
	char params[40];

	sprintf(params, "zoom %s", zoom);
	XPASet(NULL, (char*) "ds9", params, NULL, NULL, 0, &names, &messages, 1);
	if (names) free(names);
	if (messages) free(messages);
}

void XPAPreservePan(bool yes) {
	char *names, *messages;
	char params[40];

	sprintf(params, "preserve pan %s", yes ? "yes" : "no");
	XPASet(NULL, (char*) "ds9", params, NULL, NULL, 0, &names, &messages, 1);
	if (names) free(names);
	if (messages) free(messages);
}

void XPAPreserveRegion(bool yes) {
	char *names, *messages;
	char params[40];

	sprintf(params, "preserve regions %s", yes ? "yes" : "no");
	XPASet(NULL, (char*) "ds9", params, NULL, NULL, 0, &names, &messages, 1);
	if (names) free(names);
	if (messages) free(messages);
}

void DisplayImage() {// display fits image
	if (XPASetFile(state.filepath.c_str(), state.filename.c_str(), firstimg) && firstimg) {
		firstimg = false;
		XPASetScaleMode("zscale");
		XPASetZoom("to fit");
		XPAPreservePan(true);
		XPAPreserveRegion(true);
	}
}
/*==========================================================================*/
void UploadFile() {// 上传文件
	FileTransferClient::upload_file file;
	file.grid_id = param.grpid;
	file.filename = state.filename;
	file.filepath = state.filepath;
	ftcli->NewFile(&file);
}
/*==========================================================================*/
/*!
 * @brief 设置焦点目标位置
 * @param target 目标位置
 * @return
 * 处理结果. true: 需要重新定位; false: 不需要定位
 */
bool set_focus_target(int tar) {
	bool moving(false);
	focus.posTar = tar;
	if (tar != focus.posAct) {
		moving = true;
		focus.repeat = 5;

		int n;
		const char *to = mntproto->compact_focus(param.grpid, param.unitid, state.cid, tar, n);
		focus.tcp->write(to, n);
	}

	return moving;
}

/*!
 * @brief 设置焦点实际位置
 * @param act 实际位置
 * @return
 * 处理结果
 * true: 静止
 * false: 运动
 */
bool set_focus_real(int act) {
	bool rslt = act == focus.posAct;
	if (!rslt) {
		focus.posAct = act;
		PrintFocus();
	}
	return rslt;
}

/*!
 * @brief 检查是否存在下一个焦点位置执行观测序列
 * @return
 * 下一个焦点位置. 若已结束或不可调节, 则返回VALID_FOCUS
 */
int focuser_next() {
	if (!focus.tcp.use_count() || state.mode != MODE_AUTO
			|| focus.posTar == param.stroke_stop)
		return VALID_FOCUS;
	int op = focus.posTar, np;
	np = op + param.stroke_step;
	if ((np - param.stroke_stop) * (op - param.stroke_stop) < 0) np = param.stroke_stop;

	return np;
}

/*!
 * @brief 调焦器到达设定位置
 * @return
 * 0 -- 到达设定位置
 * 1 -- 未到达设定位置, 且不确定是否仍在定位
 * 2 -- 未到达设定位置, 并确定已停止定位
 * @note
 * 检测前置条件: set_focus_real()返回true
 */
int focuser_arrive() {
	if (abs(focus.posAct - focus.posTar) < param.focuser_error) return 0;
	return (--focus.repeat >= 0 ? 1 : 2);
}
/*==========================================================================*/
/*==========================================================================*/
/// 网络: 调焦器
/*!
 * @brief 处理收到的调焦信息
 * @param client 网络连接资源
 * @param ec 错误代码
 */
void ReceiveFocus(const long client, const long ec) {
	PostMessage(!ec ? 1 : 2);
}

/*!
 * @brief 收到调焦器网络连接请求
 * @param client 网络连接资源
 * @param param 参数
 */
void AcceptFocus(const tcpcptr& client, const long param) {
	focus.tcp = client;
	focus.reset();
	const tcpc_cbtype& slot = boost::bind(&ReceiveFocus, _1, _2);
	client->register_receive(slot);
	PrintXY(1, LINE_FOCUS, "focuser is on-line");

	mutex_lock lck(mtxcur);
	MovetoXY(curpos, LINE_INPUT);
	UpdateScreen();
}

/*!
 * @brief 启动调焦器网络服务
 * @return
 * 服务启动结果
 */
bool StartServerFocus() {
	const tcps_cbtype& slot = boost::bind(&AcceptFocus, _1, _2);
	tcpsfoc = boost::make_shared<tcp_server>();
	tcpsfoc->register_accept(slot);
	return tcpsfoc->start(param.portFocus);
}

/*!
 * @brief 解析更新焦点位置
 */
void ResolveFocus() {
	char term[] = "\n";        // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	mpbase proto_body;
	std::string proto_type;
	tcpcptr client = focus.tcp;
	static char buff[TCP_BUFF_SIZE];

	while (client->is_open() && (pos = client->lookup(term, len)) >= 0) {
		/* 有效性判定 */
		if ((toread = pos + len) > TCP_BUFF_SIZE) {// 原因: 遗漏换行符作为协议结束标记; 高>概率性丢包
			client->close();
			PrintXY(1, LINE_ERROR, "protocol length from focuser is over than threshold");

			mutex_lock lck(mtxcur);
			MovetoXY(curpos, LINE_INPUT);
			UpdateScreen();
		}
		else {
			/* 读取协议内容 */
			client->read(buff, toread);
			buff[pos] = 0;
			/* 解析协议 */
			proto_type = mntproto->resolve(buff, proto_body);
			if (boost::iequals(proto_type, "focus")) {
				boost::shared_ptr<mntproto_focus> proto = boost::static_pointer_cast<mntproto_focus>(proto_body);

				if (boost::iequals(proto->group_id, param.grpid)
					&& boost::iequals(proto->unit_id, param.unitid)
					&& boost::iequals(proto->camera_id, state.cid)
					&& set_focus_real(proto->position)
					&& state.mode == MODE_AUTO) {// 处理焦点位置
					int code = focuser_arrive();
					if (!code) {
						if (abs(param.stroke_start - param.stroke_back - focus.posAct) < param.focuser_error)// 空回, 消隙
							set_focus_target(param.stroke_start);
						else {// 开始曝光
							camera->Expose(state.expdur, state.imgtype == IMGTYPE_OBJECT);
						}
					}
					else if (code == 2) {
						state.mode = MODE_INIT;
						PrintXY(1, LINE_ERROR, "focuser could not arrive target position");
						mutex_lock lck(mtxcur);
						MovetoXY(curpos, LINE_INPUT);
						UpdateScreen();
					}
				}
			}
		}
	}
}

/*==========================================================================*/
void ExposeProcessCB(const double left, const double percent, const int status) {
	switch((CAMERA_STATUS) status) {
	case CAMERA_ERROR:  // 错误, 需要重启相机等操作
		PostMessage(5);
		break;
	case CAMERA_IDLE:   // 中止曝光
		PostMessage(6);
		break;
	case CAMERA_EXPOSE: // 曝光过程中
		PrintExprocess(percent);
		break;
	case CAMERA_IMGRDY: // 图像准备完成, 可以存储等操作
		PostMessage(4);
		break;
	default:
		break;
	}
}

/*!
 * @brief 将相机采集数据存储为FITS文件
 * @return
 */
bool SaveFITSFile() {
	fitsfile *fitsptr;
	int status(0);
	int naxis(2);
	boost::shared_ptr<devcam_info> nfcam = camera->GetCameraInfo();
	long naxes[] = {nfcam->roi.get_width(), nfcam->roi.get_height()};
	long pixels = nfcam->roi.get_width() * nfcam->roi.get_height();
	char buff[300];
	int n;
	std::string tmstr;

	// 创建目录结构
	if (state.frmno == 1) {
		sprintf(buff, "%s/G%s_%s_%s", param.pathroot.c_str(),
				param.unitid.c_str(), state.cid.c_str(),
				nfcam->utcdate.c_str());
		state.pathname = buff;
		if (access(state.pathname.c_str(), F_OK)) mkdir(state.pathname.c_str(), 0755);
	}
	// 生成文件名
	n = sprintf(buff, "G%s", state.cid.c_str());
	n += sprintf(buff +n, "_%s_%s.fit",
			state.imgtypeabbr.c_str(),
			nfcam->utctime.c_str());
	state.filename = buff;
	sprintf(buff, "%s/%s", state.pathname.c_str(), state.filename.c_str());
	state.filepath = buff;
	// 存储FITS文件并写入完整头信息
	fits_create_file(&fitsptr, state.filepath.c_str(), &status);
	fits_create_img(fitsptr, USHORT_IMG, naxis, naxes, &status);
	fits_write_img(fitsptr, TUSHORT, 1, pixels, nfcam->data.get(), &status);
	/* FITS头 */
	fits_write_key(fitsptr, TSTRING, "GROUP_ID", (void*)param.grpid.c_str(), "group id", &status);
	fits_write_key(fitsptr, TSTRING, "UNIT_ID", (void*)param.unitid.c_str(), "unit id", &status);
	fits_write_key(fitsptr, TSTRING, "CAM_ID", (void*)state.cid.c_str(), "camera id", &status);
	fits_write_key(fitsptr, TSTRING, "MOUNT_ID", (void*)param.unitid.c_str(), "mount id", &status);
	fits_write_key(fitsptr, TSTRING, "CCDTYPE", (void*)state.imgtypestr.c_str(), "type of image", &status);
	fits_write_key(fitsptr, TSTRING, "DATE-OBS", (void*)nfcam->dateobs.c_str(), "UTC date of begin observation", &status);
	fits_write_key(fitsptr, TSTRING, "TIME-OBS", (void*)nfcam->timeobs.c_str(), "UTC time of begin observation", &status);
	fits_write_key(fitsptr, TSTRING, "TIME-END", (void*)nfcam->timeend.c_str(), "UTC time of end observation", &status);
	fits_write_key(fitsptr, TDOUBLE, "JD", &nfcam->jd, "Julian day of begin observation", &status);
	fits_write_key(fitsptr, TDOUBLE, "EXPTIME", &nfcam->eduration, "exposure duration", &status);
	fits_write_key(fitsptr, TDOUBLE, "GAIN", &nfcam->gain, "", &status);
	fits_write_key(fitsptr, TDOUBLE, "TEMPSET", &nfcam->coolerset, "cooler set point", &status);
	fits_write_key(fitsptr, TDOUBLE, "TEMPACT", &nfcam->coolerget, "cooler actual point", &status);
	fits_write_key(fitsptr, TSTRING, "TERMTYPE", (void*)state.termtype.c_str(), "terminal type", &status);

	if (state.objname.empty())     fits_write_key(fitsptr, TSTRING, "OBJECT",   (void*)state.objname.c_str(), "name of object", &status);
	if (focus.posAct != VALID_FOCUS) fits_write_key(fitsptr, TINT,    "TELFOCUS", &focus.posAct,    "telescope focus value in micron", &status);

	fits_write_key(fitsptr, TINT, "FRAMENO", &state.frmno, "frame no in this run", &status);
	fits_close_file(fitsptr, &status);

	if (status) {
		char txt[200];
		fits_get_errstatus(status, txt);
		gLog.Write(NULL, LOG_FAULT, "Fail to save FITS file<%s>: %s", state.filepath.c_str(), txt);
	}
	return status == 0;
}

/*!
 * @brief 曝光正确结束
 */
void ExposeComplete() {
	ShowCursor(false);
	++state.frmno;
	SaveFITSFile();
	if (param.bfts && ftcli.unique() && state.mode == MODE_AUTO) UploadFile();
	if (param.display) DisplayImage();

	PrintXY(1, LINE_STATUS, "file<%d/%d>: \033[93;49m\033[1m%s\033[0m",
			state.frmno, state.frmcnt,
			state.filepath.c_str());
	PrintXY(1, LINE_EXPROCESS, "");

	if (state.frmno < state.frmcnt) {// 继续曝光
		if (state.mode != MODE_INIT)
			camera->Expose(state.expdur, state.imgtype == IMGTYPE_OBJECT);
		else {
			ClearError();
			PrintXY(1, LINE_STATUS, "exposure is aborted. %s", camera->GetCameraInfo()->errmsg.c_str());
		}
	}
	else if (state.mode != MODE_AUTO) {
		state.mode = MODE_INIT;
		PrintXY(1, LINE_ERROR, "exposure is over");
	}
	else {// 检查是否需要继续调焦并继续观测
		int tar = focuser_next();
		if (tar == VALID_FOCUS) {
			state.mode = MODE_INIT;
			PrintXY(1, LINE_ERROR, "exposure is over");
		}
		else {
			set_focus_target(tar);
			state.frmno = 0;
		}
	}

	mutex_lock lck(mtxcur);
	MovetoXY(curpos, LINE_INPUT);
	ShowCursor(true);
	UpdateScreen();
}

/*!
 * @brief 中止曝光
 */
void ExposeAbort() {
	state.mode = MODE_INIT;
	ShowCursor(false);
	ClearError();
	PrintXY(1, LINE_STATUS, "exposure is aborted. %s", camera->GetCameraInfo()->errmsg.c_str());

	mutex_lock lck(mtxcur);
	MovetoXY(curpos, LINE_INPUT);
	ShowCursor(true);
	UpdateScreen();
}

/*!
 * @brief 曝光失败
 */
void ExposeFail() {
	state.mode = MODE_INIT;
	ShowCursor(false);
	PrintXY(1, LINE_ERROR, "exposure fail. %s",
			camera->GetCameraInfo()->errmsg.c_str());

	mutex_lock lck(mtxcur);
	MovetoXY(curpos, LINE_INPUT);
	ShowCursor(true);
	UpdateScreen();
}

/*==========================================================================*/
/// 消息队列
/*!
 * @消息类型:
 * 0 - 结束消息队列相关线程
 * 1 - 收到调焦信息
 * 2 - 调焦网络远程主机断开连接
 * 3 - 焦点到位
 * 4 - 焦点长时间不能到达位置
 *
 */
/*!
 * @brief 线程, 消息机制工作逻辑
 */
void ThreadMessageQueue() {
	long msg;
	msgque::size_type recvd_size;
	msgque::size_type msg_size = sizeof(long);
	unsigned int priority;
	long pos;

	do {
		queue->receive((void*) &msg, msg_size, recvd_size, priority);
		switch(msg) {
		case 1:// 收到调焦信息
			ResolveFocus();
			break;
		case 2:// 调焦远程主机断开网络连接
			focus.tcp.reset();
			ShowCursor(false);
			if (state.mode == MODE_AUTO) {
				PrintXY(1, LINE_ERROR, "stroke sequence will be interrupted after this position over");
				state.mode = MODE_MANUAL;
			}
			PrintXY(1, LINE_FOCUS, "focuser is off-line due to remote broken");
			{
				mutex_lock lck(mtxcur);
				MovetoXY(curpos, LINE_INPUT);
				ShowCursor(true);
				UpdateScreen();
			}
			break;
		case 3:// 焦点到位
			break;
		case 4:// 曝光正确结束
			ExposeComplete();
			break;
		case 5:// 曝光失败
			ExposeFail();
			break;
		case 6:// 中止曝光
			ExposeAbort();
			break;
		default:
			break;
		}
	}while(msg != 0);
}

void SendMessage(const long msg) {// 投递高优先级消息
	if (queue.unique()) queue->send(&msg, sizeof(long), 10);
}

void PostMessage(const long msg) {// 投递低优先级消息
	if (queue.unique()) queue->send(&msg, sizeof(long), 1);
}

/*!
 * @brief 启动消息机制
 * @return
 * 消息机制启动结果
 */
bool StartMessageQueue() {
	const char* name = "msg_focaes";
	msgque::remove(name);
	queue.reset(new msgque(boost::interprocess::create_only, name, 1024, sizeof(long)));
	thrdmsg.reset(new boost::thread(boost::bind(&ThreadMessageQueue)));

	return queue.unique() && thrdmsg.unique();
}

/*!
 * @brief 中止消息机制
 */
void StopMessageQueue() {
	if (thrdmsg.unique()) {
		SendMessage(0);
		thrdmsg->join();
	}
	if (queue.unique()) msgque::remove("msg_focaes");
}
/*==========================================================================*/
//////////////////////////////////////////////////////////////////////////////
int MainBody() {// 主工作流程
	char input[100], command[100], ch;
	char *token;
	char seps[] = " ,;\t";
	int pos(0), n;

//////////////////////////////////////////////////////////////////////////////
// 准备工作环境
	param.LoadFile(gConfigPath);
	if (!StartServerFocus()) {
		gLog1.Write(LOG_FAULT, "", "Failed to create TCP server for focuser");
		return -1;
	}

	if (!StartMessageQueue()) {
		gLog1.Write(LOG_FAULT, "", "Failed to create message queue");
		return -2;
	}
	mntproto = boost::make_shared<mount_proto>();
	if (param.display) system("ds9&");
	if (param.bfts) {
		ftcli = boost::make_shared<FileTransferClient>();
		ftcli->SetHost(param.ipfts, param.portfts);
		ftcli->Start();
	}

	ClearScreen();
	PrintHelp();
	PrintServer();
	PrintAutoParameter();
	PrintInput();

//////////////////////////////////////////////////////////////////////////////
	while (1) {
		if ((ch = getchar()) != '\r' && ch !='\n') {
			input[pos++] = (char) ch;
			mutex_lock lck(mtxcur);
			++curpos;
		}
		else if (pos) {
			input[pos] = 0;
			pos = 0;
			strcpy(command, input);
			token = strtok(command, seps); // 第一个token为关键字或者其它...

			if (!(strcmp(input, "Q") && strcasecmp(input, "quit"))) break;

			if (!strcasecmp(token, "on")) {// 尝试连接相机
				if (camera.unique() && camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera had connected");
				else {// 依据参数选择待连接相机
					std::string camid; // 相机标志
					boost::format fmt("%03d");
					int cid;

					if ((token = strtok(NULL, seps)) == NULL || atoi(token) == 5) {// U9000
						fmt % (atoi(param.unitid.c_str()) * 10 + 5); // 相机编号编码格式1
						state.cid = fmt.str();
						state.termtype = "FFoV";
						boost::shared_ptr<CameraApogee> ccd = boost::make_shared<CameraApogee>();
						camera = boost::static_pointer_cast<CameraBase>(ccd);
					}
					else {// GWAC GY
						using boost::asio::ip::address_v4;
						std::string camip = token;
						address_v4 addr = address_v4::from_string(camip.c_str());
						fmt % (addr.to_ulong() % 256);
						state.cid = fmt.str();
						state.termtype = "JFoV";
						boost::shared_ptr<CameraGY> ccd = boost::make_shared<CameraGY>(camip);
						camera = boost::static_pointer_cast<CameraGY>(ccd);
					}
					if (!camera->Connect()) {
						PrintXY(1, LINE_ERROR, "failed to connect camera: %s", camera->GetCameraInfo()->errmsg.c_str());
						camera.reset();
						state.reset();
					}
					else {
						PrintXY(1, LINE_STATUS, "camera<%s> connected", state.cid.c_str());
						ClearError();
						const ExposeProcess::slot_type& slot = boost::bind(&ExposeProcessCB, _1, _2, _3);
						camera->register_expose(slot);

						if (param.bfts && ftcli.unique()) {
							ftcli->SetDeviceID(param.grpid, param.unitid, state.cid);
						}
					}
				}
			}
			else if (!strcasecmp(token, "off")) {// 尝试断开相机
				if (!camera.unique() || !camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else {
					camera->Disconnect();
					if (camera->IsConnected())
						PrintXY(1, LINE_ERROR, "failed to disconnect camera");
					else {
						PrintXY(1, LINE_STATUS, "camera disconnected");
						ClearError();
						camera.reset();
						state.reset();
					}
				}
			}
			else if (!strcasecmp(token, "b") || !strcasecmp(token, "bias")) {// 尝试拍摄本底
				if (!camera.unique() || !camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else {
					int count = -1;
					if ((token = strtok(NULL, seps)) != NULL) count = atoi(token);
					state.mode = MODE_MANUAL;
					state.set_exposure(IMGTYPE_BIAS, count);
					PrintManualParameter();
					ClearError();
					if (!camera->Expose(state.expdur, false)) {
						PrintXY(1, LINE_ERROR, "%s", camera->GetCameraInfo()->errmsg.c_str());
						state.mode = MODE_INIT;
					}
				}
			}
			else if (!strcasecmp(token, "d") || !strcasecmp(token, "dark")) {// 尝试拍摄暗场
				if (!camera.unique() || !camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else {
					int count = -1;
					double expdur = -1.0;
					if ((token = strtok(NULL, seps)) != NULL) expdur = atof(token);
					if ((token = strtok(NULL, seps)) != NULL) count = atoi(token);
					state.mode = MODE_MANUAL;
					state.set_exposure(IMGTYPE_DARK, count, expdur);
					PrintManualParameter();
					ClearError();
					if (!camera->Expose(state.expdur, false)) {
						PrintXY(1, LINE_ERROR, "%s", camera->GetCameraInfo()->errmsg.c_str());
						state.mode = MODE_INIT;
					}
				}
			}
			else if (!strcasecmp(token, "f") || !strcasecmp(token, "focus")) {// 检查或改变调焦器位置
				if (!focus.tcp.use_count())
					PrintXY(1, LINE_ERROR, "focuser is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else if (state.cid.empty())
					PrintXY(1, LINE_ERROR, "camera_id is empty, camera is required to be on-line");
				else if ((token = strtok(NULL, seps)) != NULL) {
					if (focus.posTar == VALID_FOCUS)
						set_focus_target(atoi(token));
					else
						set_focus_target(atoi(token) + focus.posTar);
					ClearError();
				}
			}
			else if (!strcasecmp(token, "r") || !strcasecmp(token, "reload")) {// 尝试重新加载配置参数
				if (state.mode != MODE_INIT) PrintXY(1, LINE_ERROR, "camera being in AUTO mode");
				else {
					param.LoadFile(gConfigPath);
					PrintAutoParameter();
					ClearError();
				}
			}
			else if (!strcasecmp(token, "start")) {// 尝试启动观测流程
				if (!focus.tcp.use_count())
					PrintXY(1, LINE_ERROR, "focuser is off-line");
				else if (!camera.unique() || !camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else {
					PrintAutoParameter();
					ClearError();
					state.mode = MODE_AUTO;
					state.set_exposure(IMGTYPE_OBJECT, param.frmcnt, param.expdur, "auto");
					if (!set_focus_target(param.stroke_start - param.stroke_back)) // 顺序执行流程. 多走一个间隔用于消齿隙
						set_focus_target(param.stroke_start);
				}
			}
			else if (!strcasecmp(token, "stop")) {// 尝试中止观测流程
				if (state.mode != MODE_INIT) {// 中止观测流程
					if (camera->GetCameraInfo()->state >= CAMERA_EXPOSE)
						camera->AbortExpose();
				}
				else PrintXY(1, LINE_ERROR, "system is idle");
			}
			else {// 尝试拍摄目标图像
				if (!camera.unique() || !camera->IsConnected())
					PrintXY(1, LINE_ERROR, "camera is off-line");
				else if (state.mode != MODE_INIT)
					PrintXY(1, LINE_ERROR, "camera being in exposure");
				else {
					std::string name = token;
					int count(-1);
					double expdur(-1.0);
					if ((token = strtok(NULL, seps)) != NULL)
						expdur = atof(token);
					if ((token = strtok(NULL, seps)) != NULL)
						count = atoi(token);
					state.mode = MODE_MANUAL;
					state.set_exposure(IMGTYPE_OBJECT, count, expdur, name);
					PrintManualParameter();
					ClearError();
					if (!camera->Expose(state.expdur, true)) {
						PrintXY(1, LINE_ERROR, "%s", camera->GetCameraInfo()->errmsg.c_str());
						state.mode = MODE_INIT;
					}
				}
			}

			PrintInput();
		}
	}

	StopMessageQueue();
	tcpsfoc.reset();
	if (camera.unique() && camera->IsConnected()) camera->Disconnect();
	if (ftcli.unique()) ftcli->Stop();

	return 0;
}

int main(int argc, char** argv) {
	int rslt(0);

	if (argc >= 2) {// 处理命令行参数
		if (strcmp(argv[1], "-d") == 0) {
			param.InitFile(gConfigPath);
		}
		else {
			printf("Usage: focaes <-d>\n");
		}
	}
	else {// 常规工作模式
		rslt = MainBody();
	}

	return rslt;
}
