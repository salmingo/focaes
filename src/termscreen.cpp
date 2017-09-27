
#include <stdarg.h>
#include <stdio.h>
#include "termscreen.h"

void ClearScreen() {
	printf("\033[2J\033[1;1H");
}

void MovetoXY(int x, int y) {
	printf("\033[%d;%dH", y, x);
}

void PrintXY(int x, int y, const char *format, ...) {
	MovetoXY(1, y);
	printf("\033[0K");	// clear line y
	MovetoXY(x, y);
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
}

void ShowCursor(bool show) {
	if (show) printf("\033[?25h");
	else      printf("\033[?25l");
}

void UpdateScreen() {
	fflush(stdout);
}
