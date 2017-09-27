/*
 * @file globaldef.h 声明全局唯一参数
 * @version 0.1
 * @date 2017/07/06
 */

#ifndef GLOBALDEF_H_
#define GLOBALDEF_H_

// 软件名称、版本与版权
#define DAEMON_NAME			"focaes"
#define DAEMON_VERSION		"v0.1 @ Sep, 2017"
#define DAEMON_AUTHORITY	"© SVOM Group, NAOC"

// 软件配置文件
const char gConfigPath[] = "focaes.xml";
// 日志文件路径与文件名前缀
const char gLogDir[]    = "log";
const char gLogPrefix[] = "focaes_";

#endif /* GLOBALDEF_H_ */
