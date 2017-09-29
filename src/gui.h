/*
 * @file gui.h 声明用户界面交互区
 */

#ifndef GUI_H_
#define GUI_H_

/**
  定义用户交互区
******************************************************************* <分割提示符>        1
On <camera ip/id>         # connect camera. empty for U9000                            2
Off                       # disconnect camera                                          3
Bias <count>              # take sequential BIAS images                                4
Dark <duration> <count>   # take sequential DARK images                                5
<name> <duration> <count> # take sequential LIGHT images                               6
Focus <position>          # change focuser position                                    7
Reload                    # Reload configuration parameters                            8
Start                     # start sequence using configuration parameters              9
Stop                      # stop running sequence                                     10
Quit                      # quit program                                              11
******************************************************************* <分割提示符>        12
Focus Daemon Port: %d                                                                 13
<duration=%.3f>, <count=%d>, <focuser from %d to %d, step=%d, error=%d>               14 控制参数
<focus=%d> <target=%d>                                                                15 焦点位置
<status>                                                                              16 状态
<error or warn prompts>                                                               17 错误提示
<exposure process>                                                                    18 曝光进度
<space line>                                                                          19
<User Input>                                                                          20

  定义用户指令
  On  <camera ip/index>     # 连接相机
  Off                       # 断开相机
  Bias <count>              # 采集本底
  Dark <duration> <count>   # 采集暗场
  <name> <duration> <count> # 采集目标图像
  Reload                    # 重新加载配置参数
 */

enum {// 各行对应信息
	LINE_SEPARATOR = 1,
	LINE_PORT = 13,
	LINE_PARAM  = 14,
	LINE_FOCUS  = 15,
	LINE_STATUS = 16,
	LINE_ERROR  = 17,
	LINE_EXPROCESS = 18,
	LINE_INPUT  = 20
};

#endif /* GUI_H_ */
