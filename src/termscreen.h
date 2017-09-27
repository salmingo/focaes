/*!
 * @file termscreen.h 在控制台上显示信息
 */

#ifndef _TERM_SCREEN_H_
#define _TERM_SCREEN_H_

/*!
 * @brief 清除屏幕
 */
extern void ClearScreen();
/*!
 * @brief 改变光标位置
 * @param x X坐标
 * @param y Y坐标
 */
extern void MovetoXY(int x, int y);
/*!
 * @brief 在(x,y)起始位置打印信息
 * @param x      X坐标
 * @param y      Y坐标
 * @param format 信息格式
 */
extern void PrintXY(int x, int y, const char *format, ...);
/*!
 * @brief 显示或隐藏光标
 * @param show true: 显示光标; false: 隐藏光标
 */
extern void ShowCursor(bool show);
/*!
 * @brief 在控制台屏幕上显示已打印信息
 */
extern void UpdateScreen();

#endif
