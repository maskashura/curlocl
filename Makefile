all:
	gcc curl.c curlocl.c curlutil.c -o curlocl -lOpenCL
