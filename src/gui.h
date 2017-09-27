/*
 * @file gui.h 声明用户界面交互区
 */

#ifndef GUI_H_
#define GUI_H_

/**
  定义用户交互区
******************************************************************* <分割提示符>        1
On <camera ip/id>         # connect camera. id==5                                      2
Off <camera id>           # disconnect camera<1-5>                                     3
Use <camera id>           # select camera<1-5>                                         4
Bias <count>              # take sequential BIAS images                                5
Dark <duration> <count>   # take sequential DARK images                                6
<name> <duration> <count> # take sequential LIGHT images                               7
Focus <position>          # change focuser position                                    8
Reload                    # Reload configuration parameters                            9
Start                     # start sequence using configuration parameters             10
Stop                      # stop running sequence                                     11
Quit                      # quit program                                              12
******************************************************************* <分割提示符>       13
<space line>                                                                          14
DaemonPort: <focus = %d>                                                              15
Camera<1: %s>, on/off, <duration=%.3f>, <focus=%d, from %d to %d>, <frame %d of %d>   16
Camera<2: %s>, on/off, <duration=%.3f>, <focus=%d, from %d to %d>, <frame %d of %d>   17
Camera<3: %s>, on/off, <duration=%.3f>, <focus=%d, from %d to %d>, <frame %d of %d>   18
Camera<4: %s>, on/off, <duration=%.3f>, <focus=%d, from %d to %d>, <frame %d of %d>   19
Camera<5: %s>, on/off, <duration=%.3f>, <focus=%d, from %d to %d>, <frame %d of %d>   20
<space line>                                                                          21
<error or warn prompts>                                                               22
<error or warn prompts for camera 1>                                                  23
<error or warn prompts for camera 2>                                                  24
<error or warn prompts for camera 3>                                                  25
<error or warn prompts for camera 4>                                                  26
<error or warn prompts for camera 5>                                                  27
<space line>                                                                          28
<User Input>                                                                          29

  定义用户指令
  On  <camera ip/index>     # 连接相机. index==5
  Off <camera id>           # 断开相机
  Use <camera id>           # 选择相机, 指令对应该相机
  Bias <count>              # 采集本底
  Dark <duration> <count>   # 采集暗场
  <name> <duration> <count> # 采集目标图像
  Reload                    # 重新加载配置参数
 */

enum {// 各行对应信息
	LINE_SEPARATOR = 1,
	LINE_PORT = 15,
	LINE_CAMERA_1 = 16,
	LINE_STATUS = 22,
	LINE_STATUS_1 = 23,
	LINE_INPUT  = 29
};

#endif /* GUI_H_ */
