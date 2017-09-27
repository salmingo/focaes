/*
 * @file GLog.cpp 类GLog的定义文件
 * @version      2.0
 * @date    2016年10月28日
 */

#include <sys/stat.h>
#include <sys/types.h>	// Linux需要
#include <unistd.h>
#include <stdarg.h>
#include <string>
#include "GLog.h"
#include "globaldef.h"

using namespace std;
using namespace boost::posix_time;

GLog::GLog(FILE *out) {
	m_day = -1;
	m_fd  = out;
}

GLog::~GLog() {
	if (m_fd && m_fd != stdout && m_fd != stderr) fclose(m_fd);
}

bool GLog::valid_file(ptime &t) {
	if (m_fd == stdout || m_fd == stderr) return true;
	ptime::date_type date = t.date();
	if (m_day != date.day()) {// 日期变更
		m_day = date.day();
		if (m_fd) {// 关闭已打开的日志文件
			fprintf(m_fd, "%s continue\n", string(69, '>').c_str());
			fclose(m_fd);
			m_fd = NULL;
		}
	}

	if (m_fd == NULL) {
		char pathname[200];

		if (access(gLogDir, F_OK)) mkdir(gLogDir, 0755);	// 创建目录
		sprintf(pathname, "%s/%s%s.log",
				gLogDir, gLogPrefix, to_iso_string(date).c_str());
		m_fd = fopen(pathname, "a+");
		fprintf(m_fd, "%s\n", string(79, '-').c_str());
	}

	return (m_fd != NULL);
}

void GLog::Write(const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(m_mutex);
	ptime t(microsec_clock::local_time());

	if (valid_file(t)) {
		// 时间标签
		fprintf(m_fd, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		// 日志描述的格式与内容
		va_list vl;
		va_start(vl, format);
		vfprintf(m_fd, format, vl);
		va_end(vl);
		fprintf(m_fd, "\n");
		fflush(m_fd);
	}
}

void GLog::Write(const LOG_TYPE type, const char* where, const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(m_mutex);
	ptime t(microsec_clock::local_time());

	if (valid_file(t)) {
		// 时间标签
		fprintf(m_fd, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		// 日志类型
		if (type == LOG_WARN)       fprintf(m_fd, "WARN: ");
		else if (type == LOG_FAULT) fprintf(m_fd, "ERROR: ");
		// 事件位置
		if (where) fprintf(m_fd, "%s, ", where);
		// 日志描述的格式与内容
		va_list vl;
		va_start(vl, format);
		vfprintf(m_fd, format, vl);
		va_end(vl);
		fprintf(m_fd, "\n");
		fflush(m_fd);
	}
}
