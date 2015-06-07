/*
 * test.c
 *
 *  Created on: 2014年12月23日
 *      Author: root
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <pthread.h>
#include <poll.h>
#include <semaphore.h>

char lcd_path[] = "/dev/fb0";
char fimc0_path[] = "/dev/video1";
char cam_path[] = "/dev/video0";

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int lcd_buf_size;
static char *fb_buf = NULL;

int fimc0_fd;
int lcd_fd;
int cam_fd;

#define CapWidth 640
#define CapHeight 480
#define PIXELFMT V4L2_PIX_FMT_YUYV
#define ERRSTR strerror(errno)
#define CLEAR(x)    memset(&(x), 0, sizeof(x))
#define ERR(...) __info("Error", __FILE__, __LINE__, __VA_ARGS__)
#define ERR_ON(cond, ...) ((cond) ? ERR(__VA_ARGS__) : 0)
static inline int __info(const char *prefix, const char *file, int line,
		const char *fmt, ...) {
	int errsv = errno;
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "%s(%s:%d): ", prefix, file, line);
	vfprintf(stderr, fmt, va);
	va_end(va);
	errno = errsv;

	return 1;
}
static int fimc0_cap_index = 0;

#define CHECKNUM 8

struct {
	unsigned int type;
	char *name;
} enum_fmt[] = { { V4L2_CAP_VIDEO_CAPTURE, "V4L2_CAP_VIDEO_CAPTURE" }, {
V4L2_CAP_VIDEO_OUTPUT, "V4L2_CAP_VIDEO_OUTPUT" }, {
V4L2_CAP_VIDEO_OVERLAY, "V4L2_CAP_VIDEO_OVERLAY" }, { 0x00001000,
		"V4L2_CAP_VIDEO_CAPTURE_MPLANE" }, { 0x00002000,
		"V4L2_CAP_VIDEO_OUTPUT_MPLANE" }, { 0x00008000,
		"V4L2_CAP_VIDEO_M2M_MPLANE" }, { 0x00004000, "V4L2_CAP_VIDEO_M2M" }, {
V4L2_CAP_STREAMING, "V4L2_CAP_STREAMING" }, };

char *temp_buf = NULL;
int open_camera_device() {
	int fd;

	if ((fd = open(cam_path, O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		exit(EXIT_FAILURE);
	}
	cam_fd = fd;
	if ((fimc0_fd = open(fimc0_path, O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		exit(EXIT_FAILURE);
	}

	printf("open cam success %d\n", cam_fd);
	printf("open fimc0_out success %d\n", fimc0_fd);

	return fd;
}
int fimc0_setfmt() {
	int err;
	int ret;

	struct v4l2_capability cap;
	struct v4l2_format stream_fmt;

	printf("%s: +\n", __func__);
	CLEAR(cap);

	ret = ioctl(fimc0_fd, VIDIOC_QUERYCAP, &cap);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_QUERYCAP: %s\n", ERRSTR))
		return -errno;

	printf("\ncheck the support capabilities\n");
	int i;
	for (i = 0; i < CHECKNUM; i++) {
		if (cap.capabilities & enum_fmt[i].type)
			printf("%s\n", enum_fmt[i].name);
	}
	printf("\n");

	CLEAR(stream_fmt);
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* get format from cam */
	ret = ioctl(cam_fd, VIDIOC_G_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR))
		return -errno;
	printf("%s -cam VIDIOC_G_FMT ok+\n", __func__);
	printf("cam w:%d h:%d pixformat \n", stream_fmt.fmt.pix.width,
			stream_fmt.fmt.pix.height);

	//stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//stream_fmt.fmt.pix.width = CapWidth;
	//stream_fmt.fmt.pix.height = CapHeight;
	//stream_fmt.fmt.pix.pixelformat = PIXELFMT;

	CLEAR(stream_fmt);
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	/* get format from fimc0 */
	ret = ioctl(fimc0_fd, VIDIOC_G_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR))
		return -errno;
	printf("%s -fimc0 VIDIOC_G_FMT ok+\n", __func__);
	printf("fimc0 w:%d h:%d pixformat= %c%c%c%c \n", stream_fmt.fmt.pix.width,
				stream_fmt.fmt.pix.height, stream_fmt.fmt.pix.pixelformat & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 8) & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 16) & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 24) & 0xff);

	/* setup format for FIMC 0 */
	/* keep copy of format for to-mplane conversion */

	CLEAR(stream_fmt);
	////stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	stream_fmt.fmt.pix.width = CapWidth;
	stream_fmt.fmt.pix.height = CapHeight;
	stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	printf("%s -fimc0 ready to set+\n", __func__);

	ret = ioctl(fimc0_fd, VIDIOC_S_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_FMT: %s\n", ERRSTR))
		return -errno;

	printf("%s -fimc0 set done+\n", __func__);

	/* get format from fimc0 again */
	ret = ioctl(fimc0_fd, VIDIOC_G_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR))
		return -errno;
	printf("%s -fimc0 VIDIOC_G_FMT ok+\n", __func__);
	printf("fimc0 w:%d h:%d pixformat= %c%c%c%c \n", stream_fmt.fmt.pix.width,
			stream_fmt.fmt.pix.height, stream_fmt.fmt.pix.pixelformat & 0xff,
			(stream_fmt.fmt.pix.pixelformat >> 8) & 0xff,
			(stream_fmt.fmt.pix.pixelformat >> 16) & 0xff,
			(stream_fmt.fmt.pix.pixelformat >> 24) & 0xff);
	/* set format on fimc0 capture */
	////stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	CLEAR(stream_fmt);

	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream_fmt.fmt.pix.width = 800;
	stream_fmt.fmt.pix.height = 480;
	stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

	////dump_format("pre-fimc0-capture", &stream_fmt);
	ret = ioctl(fimc0_fd, VIDIOC_S_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_FMT: %s\n", ERRSTR))
		return -errno;
	printf("%s -\n", __func__);

}
int main() {
	open_camera_device();
	fimc0_setfmt();

	while (1) {
		sleep(1);
	}
	return 0;
}
