/*
 * @file parameter.h 软件配置文件
 * @version 0.1
 * @date 2017/07/06
 * - 要求: 相机标志采用xy格式编码. x为单元标志, y为相机在单元中的索引编号<1-5>
 */

#ifndef PARAMETER_H_
#define PARAMETER_H_

#include <string>
#include <vector>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <stdlib.h>

struct param_config {// 软件配置参数
	int portFocus;		//< 面向调焦器网络服务端口
	std::string grpid;	//< 组标志
	std::string unitid;	//< 单元标志
	int stroke_start;	//< 行程起点, 量纲: 微米
	int stroke_stop;	//< 行程终点, 量纲: 微米
	int stroke_step;	//< 行程步长, 量纲: 微米
	int focuser_error;	//< 调焦器定位误差, 量纲: 微米
	double expdur;		//< 曝光时间, 量纲: 秒
	int frmcnt;			//< 曝光帧数
	bool display;		//< 是否实时显示图像
	std::string pathroot;//< 文件存储根路径

public:
	void InitFile(const std::string &filepath) {
		using boost::property_tree::ptree;

		ptree pt;
		pt.add("version", "0.1");
		pt.add("PortFocus", portFocus  = 4012);
		pt.add("Device.<xmlattr>.group", grpid  = "001");
		pt.add("Device.<xmlattr>.unit",  unitid = "001");
		pt.add("stroke.<xmlattr>.start", stroke_start = -100);
		pt.add("stroke.<xmlattr>.stop",  stroke_stop = 100);
		pt.add("stroke.<xmlattr>.step",  stroke_step = 10);
		pt.add("stroke.<xmlattr>.error", focuser_error = 2);
		pt.add("exposure.<xmlattr>.duration", expdur = 5);
		pt.add("exposure.<xmlattr>.count", frmcnt = 1);
		pt.add("display", display = false);
		pt.add("PathRoot", pathroot = "/data");

		boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filepath, pt, std::locale(), settings);
	}

	void LoadFile(const std::string &filepath) {
		using boost::property_tree::ptree;

		std::string value;
		ptree pt;
		read_xml(filepath, pt, boost::property_tree::xml_parser::trim_whitespace);

		portFocus  = pt.get("PortFocus", 4012);
		grpid  = pt.get("Device.<xmlattr>.group", "001");
		unitid = pt.get("Device.<xmlattr>.unit",  "001");
		stroke_start = pt.get("stroke.<xmlattr>.start", -100);;
		stroke_stop = pt.get("stroke.<xmlattr>.stop",  100);
		stroke_step = pt.get("stroke.<xmlattr>.step",  10);
		focuser_error = pt.get("stroke.<xmlattr>.error", 2);
		expdur = pt.get("exposure.<xmlattr>.duration", 2);
		frmcnt = pt.get("exposure.<xmlattr>.count", 3);
		display = pt.get("display", false);
		pathroot= pt.get("PathRoot", "/data");
		boost::trim_right_if(pathroot, boost::is_punct() || boost::is_space());

		if (stroke_step == 0) stroke_step = 10;
		if (focuser_error <= 0) focuser_error = 2;
		if ((stroke_start > stroke_stop && stroke_step > 0)
				|| (stroke_start < stroke_stop && stroke_step < 0))
			stroke_step *= -1;
		if (expdur <= 1E-6) expdur = 2.0;
		if (frmcnt <= 0) frmcnt = 1;
	}
};

#endif /* PARAMETER_H_ */
