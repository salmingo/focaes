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
gain <index>              # change gain                                                4
Bias <count>              # take sequential BIAS images                                5
Dark <duration> <count>   # take sequential DARK images                                6
<name> <duration> <count> # take sequential LIGHT images                               7
Focus <position>          # change focuser position                                    8
Reload                    # Reload configuration parameters                            9
Start                     # start sequence using configuration parameters             10
Stop                      # stop running sequence                                     11
Quit                      # quit program                                              12
******************************************************************* <分割提示符>        13
Focus Daemon Port: %d                                                                 14
<duration=%.3f>, <count=%d>, <focuser from %d to %d, step=%d, error=%d>               15 控制参数
<focus=%d> <target=%d>                                                                16 焦点位置
<status>                                                                              17 状态
<error or warn prompts>                                                               18 错误提示
<exposure process>                                                                    19 曝光进度
<space line>                                                                          20
<User Input>                                                                          21

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
	LINE_PORT = 14,
	LINE_PARAM,
	LINE_FOCUS,
	LINE_STATUS,
	LINE_ERROR,
	LINE_EXPROCESS,
	LINE_INPUT  = 21
};

#endif /* GUI_H_ */
