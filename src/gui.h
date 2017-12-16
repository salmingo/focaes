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
Reboot                    # reboot camera                                              4
gain <index>              # change gain                                                5
Bias <count>              # take sequential BIAS images                                6
Dark <duration> <count>   # take sequential DARK images                                7
<name> <duration> <count> # take sequential LIGHT images                               8
Focus <position>          # change focuser position                                    9
Reload                    # Reload configuration parameters                           10
Start                     # start sequence using configuration parameters             11
Stop                      # stop running sequence                                     12
Quit                      # quit program                                              13
******************************************************************* <分割提示符>        14
Focus Daemon Port: %d                                                                 15
<duration=%.3f>, <count=%d>, <focuser from %d to %d, step=%d, error=%d>               16 控制参数
<focus=%d> <target=%d>                                                                17 焦点位置
<status>                                                                              18 状态
<error or warn prompts>                                                               19 错误提示
<exposure process>                                                                    20 曝光进度
<space line>                                                                          21
<User Input>                                                                          22

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
	LINE_PORT = 15,
	LINE_PARAM,
	LINE_FOCUS,
	LINE_STATUS,
	LINE_ERROR,
	LINE_EXPROCESS,
	LINE_INPUT  = 22
};

#endif /* GUI_H_ */
