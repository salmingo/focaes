/*
 * @file parameter.h 软件配置文件
 * @version 0.1
 * @date 2017/07/06
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

struct param_camera {// 相机相关参数
	std::string camid;	//< 相机标志
	int stroke_start;	//< 行程起点, 量纲: 微米
	int stroke_stop;	//< 行程终点, 量纲: 微米
	int stroke_step;	//< 行程步长, 量纲: 微米
	int focuser_error;	//< 调焦器定位误差, 量纲: 微米
	double expdur;		//< 曝光时间, 量纲: 秒
	int frmcnt;			//< 曝光帧数

public:
	param_camera() {
		stroke_start = -100;
		stroke_stop  = 100;
		stroke_step  = 10;
		focuser_error = 2;
		expdur = 2.0;
		frmcnt = 3;
	}

	param_camera &operator=(const param_camera &other) {
		if (this != &other) {
			camid = other.camid;
			stroke_start  = other.stroke_start;
			stroke_stop   = other.stroke_stop;
			stroke_step   = other.stroke_step;
			focuser_error = other.focuser_error;
			expdur        = other.expdur;
			frmcnt        = other.frmcnt;
		}

		return *this;
	}
};

struct param_config {// 软件配置参数
	int portFocus;		//< 面向调焦器网络服务端口
	std::string grpid;		//< 组标志
	std::string unitid;		//< 单元标志
	std::vector<param_camera> camera;	//< 相机参数

public:
	void InitFile(const std::string &filepath) {
		using boost::property_tree::ptree;

		ptree pt;

		pt.put("version", "0.1");
		pt.put("PortFocus",    portFocus  = 4012);
		pt.put("group_id", grpid  = "001");
		pt.put("unit_id",  unitid = "001");

		for (int i = 1; i <= 5; ++i) {
			param_camera onecam;
			boost::format fmt("%03d");
			onecam.camid = (fmt % i).str();

			ptree node;
			node.put("stroke.<xmlattr>.start", onecam.stroke_start);
			node.put("stroke.<xmlattr>.stop",  onecam.stroke_stop);
			node.put("stroke.<xmlattr>.step",  onecam.stroke_step);
			node.put("stroke.<xmlattr>.error", onecam.focuser_error);
			node.put("exposure.<xmlattr>.duration", onecam.expdur);
			node.put("exposure.<xmlattr>.count", onecam.frmcnt);

			ptree &a = pt.add_child("camera", node);
			a.put("<xmlattr>.id", onecam.camid);

			camera.push_back(onecam);
		}

		boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filepath, pt, std::locale(), settings);
	}

	void LoadFile(const std::string &filepath) {
		using boost::property_tree::ptree;

		std::string value;
		ptree pt;
		read_xml(filepath, pt, boost::property_tree::xml_parser::trim_whitespace);

		camera.clear();
		portFocus  = pt.get("PortFocus",    4012);
		grpid  = pt.get("group_id", "001");
		unitid = pt.get("unit_id",  "001");
		BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
			if (boost::iequals(child.first, "camera")) {
				param_camera onecam;
				onecam.camid = child.second.get("<xmlattr>.id", "000");
				onecam.stroke_start = child.second.get("stroke.<xmlattr>.start", -100);;
				onecam.stroke_stop = child.second.get("stroke.<xmlattr>.stop",  100);
				onecam.stroke_step = child.second.get("stroke.<xmlattr>.step",  10);
				onecam.focuser_error = child.second.get("stroke.<xmlattr>.error", 2);
				onecam.expdur = child.second.get("exposure.<xmlattr>.duration", 2);
				onecam.frmcnt = child.second.get("exposure.<xmlattr>.count", 3);
				if (onecam.stroke_start > onecam.stroke_stop) onecam.stroke_step *= -1;

				camera.push_back(onecam);
			}
		}
	}
};

#endif /* PARAMETER_H_ */
