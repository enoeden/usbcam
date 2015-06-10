/************************************************************
 * Copyright (C), 2009-2015
 * FileName:		//
 * Author:			//
 * Date:			//
 * Description:		//
 * Version:			//
 * Function List:	//
 *     1. -------
 * History:			//
 *     <author>  <time>   <version >   <desc>
 *       WXY     4/1/15     1.0
 *       wxy     15-6-5 14：56     1.1        此版本在32bitlcd内核直接显示
 *       											        下一步 包装此模块 自适应不同usb口  自适应不同lcd 位数
 *       											        并加入opencv
 ***********************************************************/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <pthread.h>
#include <poll.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "videodev2_samsung.h"
#include "debug.h"

#include "log.h"
#include "lcd.h"
#define LEVEL 1

#define TimeOut 5

#define CapNum 109

#define CapWidth	640
#define CapHeight	480

#define ReqButNum 3

#define IsRearCamera 0

#define  FPS 20

#define PIXELFMT V4L2_PIX_FMT_YUYV

#define CapDelay 100 * 1000

typedef struct {
	void *start;
	int length;
	int bytesused;
} BUFTYPE;

BUFTYPE *fimc0_src_buf;
BUFTYPE *cam_buffers;
BUFTYPE *fimc0_dst_buf;

char lcd_path[] = "/dev/fb0";
char fimc0_path[] = "/dev/video1";
char cam_path[] = "/dev/video0";
void * cambuf = NULL;
#define CHECKNUM 8

//用于测试是否具备对应能力
struct cap {
	unsigned int type;
	const char *name;
} enum_fmt[] = { { V4L2_CAP_VIDEO_CAPTURE, "V4L2_CAP_VIDEO_CAPTURE" }, {
		V4L2_CAP_VIDEO_OUTPUT, "V4L2_CAP_VIDEO_OUTPUT" }, {
		V4L2_CAP_VIDEO_OVERLAY, "V4L2_CAP_VIDEO_OVERLAY" }, { 0x00001000,
		"V4L2_CAP_VIDEO_CAPTURE_MPLANE" }, { 0x00002000,
		"V4L2_CAP_VIDEO_OUTPUT_MPLANE" }, { 0x00008000,
		"V4L2_CAP_VIDEO_M2M_MPLANE" }, { 0x00004000, "V4L2_CAP_VIDEO_M2M" }, {
		V4L2_CAP_STREAMING, "V4L2_CAP_STREAMING" } };



static int n_buffer = 0;
void *fimc_in = NULL;
void *fimc_out = NULL;

int fimc0_src_buf_length;
int fimc0_dst_buf_length;

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int lcd_buf_size;
static char *fb_buf = NULL;

static int fimc0_fd;
int lcd_fd;
int cam_fd;
int display_x = 0;
int display_y = 0;

char *temp_buf = NULL;

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int display_format(int pixelformat) {
	printf("{pixelformat = %c%c%c%c}\n", pixelformat & 0xff,
			(pixelformat >> 8) & 0xff, (pixelformat >> 16) & 0xff,
			(pixelformat >> 24) & 0xff);
	return 0;
}

/*struct format {
	unsigned long fourcc;
	unsigned long width;
	unsigned long height;
};*/

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int open_camera_device() {
	int fd;

	if ((fd = open(cam_path, O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		exit( EXIT_FAILURE);
	}
	cam_fd = fd;
	if ((fimc0_fd = open(fimc0_path, O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		exit( EXIT_FAILURE);
	}
	log(DEBUG,"open cam & fimc0 success %d\n", fd);
	//printf("open cam & fimc0 success %d\n", fd);
	return fd;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int open_lcd_device() {
	int fd;

	int ret;
	if ((fd = open(lcd_path, O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		exit( EXIT_FAILURE);
	}
	log(DEBUG,"open lcd success %d\n", fd);

	if (-1 == ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Fail to ioctl:FBIOGET_FSCREENINFO\n");
		exit( EXIT_FAILURE);
	}
	if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("Fail to ioctl:FBIOGET_VSCREENINFO\n");
		exit( EXIT_FAILURE);
	}
	lcd_buf_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	log(INFO,
				"vinfo.xres:%d, vinfo.yres:%d, vinfo.bits_per_pixel:%d, lcd_buf_size:%d, finfo.line_length:%d\n",
				vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, lcd_buf_size,
				finfo.line_length);

	lcd_fd = fd;

	vinfo.activate = FB_ACTIVATE_FORCE;
	vinfo.yres_virtual = vinfo.yres;
	ret = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
	if (ret < 0) {
		log(DEBUG,"ioctl FBIOPUT_VSCREENINFO failed\n");
		return -1;
	}

	//mmap framebuffer
	fb_buf = (char *) mmap(
	NULL, lcd_buf_size,
	PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
	if ( NULL == fb_buf) {
		perror("Fail to mmap fb_buf");
		exit( EXIT_FAILURE);
	}
	ret = ioctl(lcd_fd, FBIOBLANK, FB_BLANK_UNBLANK);
	if (ret < 0) {
		log(ERROR,"ioctl FBIOBLANK failed\n");
		return -1;
	}
	log(INFO,"fb address %lx\n", (long int)fb_buf);
	return fd;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int cam_setfmt() {
	int ret;
	struct v4l2_fmtdesc fmt;
	struct v4l2_capability cap;
	struct v4l2_format stream_fmt;

	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = ioctl(cam_fd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
		fmt.index++;
		display_format(fmt.pixelformat);
	}
	ret = ioctl(cam_fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		perror("FAIL to ioctl VIDIOC_QUERYCAP");
		exit( EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		log(ERROR,"The Current device is not a video capture device\n");
		exit( EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		log(ERROR,"The Current device does not support streaming i/o\n");
		exit( EXIT_FAILURE);
	}

	CLEAR(stream_fmt);
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream_fmt.fmt.pix.width = CapWidth;
	stream_fmt.fmt.pix.height = CapHeight;
	stream_fmt.fmt.pix.pixelformat = PIXELFMT;
	stream_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (-1 == ioctl(cam_fd, VIDIOC_S_FMT, &stream_fmt)) {
		log(ERROR,"Can't set the fmt\n");
		perror("Fail to ioctl\n");
		exit( EXIT_FAILURE);
	}
	log(DEBUG,"VIDIOC_S_FMT successfully\n");

	log(DEBUG,"%s: -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int cam_setrate() {
	int err;

	struct v4l2_streamparm stream;

	CLEAR(stream);
	stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream.parm.capture.capturemode = 0;
	stream.parm.capture.timeperframe.numerator = 1;
	stream.parm.capture.timeperframe.denominator = FPS;

	err = ioctl(cam_fd, VIDIOC_S_PARM, &stream);
	if (err < 0) {
		log(ERROR,"FimcV4l2 start: error %d, VIDIOC_S_PARM", err);
	}

	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int cam_reqbufs() {
	struct v4l2_requestbuffers req;
	int i;
	log(DEBUG,"%s: +\n", __func__);
	CLEAR(req);

	req.count = ReqButNum;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == ioctl(cam_fd, VIDIOC_REQBUFS, &req)) {
		if ( EINVAL == errno) {
			fprintf( stderr, "%s does not support "
					"user pointer i/o\n", "campture");
			exit( EXIT_FAILURE);
		} else {
			log(INFO,"VIDIOC_REQBUFS");
			exit( EXIT_FAILURE);
		}
	}

	cam_buffers = (BUFTYPE*)calloc( ReqButNum, sizeof(*cam_buffers));

	if (!cam_buffers) {
		fprintf( stderr, "Out of memory\n");
		exit( EXIT_FAILURE);
	}

	for (i = 0; i < ReqButNum; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (-1 == ioctl(cam_fd, VIDIOC_QUERYBUF, &buf)) {
			perror("Fail to ioctl : VIDIOC_QUERYBUF");
			exit( EXIT_FAILURE);
		}

		cam_buffers[i].length = buf.length;
		cam_buffers[i].start = mmap(
		NULL, /*start anywhere*/
		buf.length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED, cam_fd, buf.m.offset);

		if ( MAP_FAILED == cam_buffers[i].start) {
			perror("Fail to mmap\n");
			log(ERROR,"%d\n", i);
			exit( EXIT_FAILURE);
		}
		log(DEBUG,"cam_capture rebuf::%lx\n", (long) cam_buffers[i].start);
	}

	log(DEBUG,"%s: -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int fimc0_setfmt() {
	int ret;
	struct v4l2_capability cap;
	struct v4l2_format stream_fmt;
	struct v4l2_crop crop;
	log(DEBUG,"%s: +\n", __func__);
	CLEAR(cap);

	ret = ioctl(fimc0_fd, VIDIOC_QUERYCAP, &cap);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_QUERYCAP: %s\n", ERRSTR)) {
		return -errno;
	}

	log(INFO,"\ncheck the support capabilities\n");
	int i;
	for (i = 0; i < CHECKNUM; i++) {
		if (cap.capabilities & enum_fmt[i].type) {
			log(INFO,"%s\n", enum_fmt[i].name);
		}
	}

	CLEAR(stream_fmt);
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* get format from cam */
	ret = ioctl(cam_fd, VIDIOC_G_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR)) {
		return -errno;
	}
	log(DEBUG,"%s -cam VIDIOC_G_FMT ok+\n", __func__);
	log(INFO,"cam w:%d h:%d pixformat \n", stream_fmt.fmt.pix.width,
			stream_fmt.fmt.pix.height);

	CLEAR(stream_fmt);
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	/* get format from fimc0 */
	ret = ioctl(fimc0_fd, VIDIOC_G_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR)) {
		return -errno;
	}
	log(DEBUG,"%s -fimc0 VIDIOC_G_FMT ok+\n", __func__);
	log(DEBUG,"fimc0 w:%d h:%d pixformat= %c%c%c%c \n", stream_fmt.fmt.pix.width,
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
	stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	log(DEBUG,"%s -fimc0 ready to set+\n", __func__);

	ret = ioctl(fimc0_fd, VIDIOC_S_FMT, &stream_fmt);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_FMT: %s\n", ERRSTR)) {
		return -errno;
	}

	log(DEBUG,"%s -fimc0 set done+\n", __func__);

	CLEAR(stream_fmt);
		stream_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		/* get format from fimc0 */
		ret = ioctl(fimc0_fd, VIDIOC_G_FMT, &stream_fmt);
		if (ERR_ON(ret < 0, "cam: VIDIOC_G_FMT: %s\n", ERRSTR)) {
			return -errno;
		}
		log(INFO,"%s -fimc0 VIDIOC_G_FMT \n", __func__);
		log(INFO,"fimc0 w:%d h:%d pixformat= %c%c%c%c \n", stream_fmt.fmt.pix.width,
				stream_fmt.fmt.pix.height, stream_fmt.fmt.pix.pixelformat & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 8) & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 16) & 0xff,
				(stream_fmt.fmt.pix.pixelformat >> 24) & 0xff);

	/* set format on fimc0 capture */
	/*dst fmt will be determined by stream_fmt.fmt.pix.field this will be RGB32 but lcd support only RGB565 now.*/

	CLEAR(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.height = stream_fmt.fmt.pix.height;
	crop.c.width = stream_fmt.fmt.pix.width;
	ret = ioctl(fimc0_fd, VIDIOC_S_CROP, &crop);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_S_CROP: %s\n", ERRSTR)) {
		return -errno;
	}

	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int fimc0_reqbufs() {
	int ret;
	struct v4l2_control ctrl;
	struct v4l2_framebuffer fb;
	struct v4l2_requestbuffers rb;
	struct v4l2_format fmt;
	CLEAR(rb);
	struct v4l2_buffer b;
	CLEAR(b);
	int n;
	log(DEBUG,"%s: +\n", __func__);
	/*dst */

	fimc0_dst_buf = (BUFTYPE*)calloc( ReqButNum, sizeof(BUFTYPE));
	if (ERR_ON(fimc0_dst_buf == NULL, "fimc0_cap_buf: VIDIOC_QUERYBUF: %s\n",
			ERRSTR)) {
		return -errno;
	}

	/*VIDIOC_S_FBUF to set ctx->fbuf info*/
	/*关于dst format  有函数fimc_outdev_set_format()设置
	 * 对于FIMC_OVLY_DMA_AUTO 及 FIMC_OVLY_DMA_MANUAL 驱动 更具 ctx->pix.field == V4L2_FIELD_NONE 等 选择
	 * pixfmt.pixelformat = V4L2_PIX_FMT_RGB32 没有RGB16的选项
	  * 对于 FIMC_OVLY_NONE_SINGLE_BUF:FIMC_OVLY_NONE_MULTI_BUF: 驱动根据 pixfmt.pixelformat = ctx->fbuf.fmt.pixelformat;、
	  * 最终调用 fimc_outdev_set_dst_format(ctrl, &pixfmt);
	 * */

	CLEAR(fb);
	fb.fmt.height = 480;
	fb.fmt.width = 800;
	if (-1 == ioctl(fimc0_fd, VIDIOC_S_FBUF, &fb)) {
		perror("VIDIOC_S_framebuffer");
		exit( EXIT_FAILURE);
	}

	/*S_FMT AS OVERLAY to get ctx->win info*/
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.height = 480;
	fmt.fmt.win.w.width = 400;
	fmt.fmt.win.w.left = 0;
	fmt.fmt.win.w.top = 0;
	if (-1 == ioctl(fimc0_fd, VIDIOC_S_FMT, &fmt)) {
		perror("VIDIOC_S_overlay_fmt");
		exit( EXIT_FAILURE);
	}
	CLEAR(b);
	CLEAR(rb);
	/*S_CTRL to get 3 vaddr*/
	CLEAR(ctrl);
	ctrl.id = V4L2_CID_OVERLAY_AUTO;
	ctrl.value = 0;

	if (-1 == ioctl(fimc0_fd, VIDIOC_S_CTRL, &ctrl)) {
		perror("VIDIOC_S_CTRL");
		exit( EXIT_FAILURE);
	}

	for (n = 0; n < ReqButNum; ++n) {
		ctrl.id = V4L2_CID_OVERLAY_VADDR0 + n;
		if (-1 == ioctl(fimc0_fd, VIDIOC_G_CTRL, &ctrl)) {
			perror("VIDIOC_G_CTRL");
			exit( EXIT_FAILURE);
		}
		fimc0_dst_buf[n].start = (void *) ctrl.value;
		log(INFO,"fimc0 dst vaddr%d :%lx\n", n, (long) fimc0_dst_buf[n].start);
		if (!fimc0_dst_buf[n].start) {
			log(ERROR,"Failed mmap buffer %d for %d\n", n, fimc0_fd);
			return -1;
		}
	}
	fimc0_dst_buf_length = 800 * 480 * 4;
	log(INFO,"fimc0 :dst buf .length:%d\n", fimc0_dst_buf_length);

	/* request buffers for FIMC0 */
	rb.count = ReqButNum;
	rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	rb.memory = V4L2_MEMORY_MMAP;
	//rb.memory = V4L2_MEMORY_USERPTR;
	ret = ioctl(fimc0_fd, VIDIOC_REQBUFS, &rb);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR)) {
		return -errno;
	}
	log(INFO,"fimc0 output_buf_num:%d\n", rb.count);

	n_buffer = rb.count;

	fimc0_src_buf = (BUFTYPE*)calloc(rb.count, sizeof(BUFTYPE));
	if (fimc0_src_buf == NULL) {
		fprintf( stderr, "Out of memory\n");
		exit( EXIT_FAILURE);
	}
	log(DEBUG,"%s, fimc0_src_buf request successfully\n", __func__);

	/* mmap DMABUF */
	for (n = 0; n < ReqButNum; ++n) {
		b.index = n;
		b.length = 1; //set by driver
		b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		b.memory = V4L2_MEMORY_MMAP;
		b.m.offset = 0; //set by driver  guess
		ret = ioctl(fimc0_fd, VIDIOC_QUERYBUF, &b);

		if (ERR_ON(ret < 0, "fimc0: VIDIOC_REQBUFS: %s\n", ERRSTR)) {
			exit( EXIT_FAILURE);
		}

		log(INFO,"fimc0 querybuf %d: %d,%d\n", n, b.length, b.m.offset);
		fimc0_src_buf[n].start = mmap( NULL, b.length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED, fimc0_fd, b.m.offset);

		fimc0_src_buf[n].length = b.length;
		if (fimc0_src_buf[n].start == MAP_FAILED) {
			log(ERROR,"Failed mmap buffer %d for %d\n", n, fimc0_fd);
			return -1;
		}

		fimc0_src_buf_length = b.length;
		log(INFO,"fimc0 querybuf:0x%08lx,%d,%d\n", (long int)fimc0_src_buf[n].start,
				fimc0_src_buf_length, b.m.offset);

	}

	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int init_device() {
	cam_setfmt();
	cam_reqbufs();
	fimc0_setfmt();
	fimc0_reqbufs();

	////cam_setrate();
	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int start_capturing(int cam_fd) {
	unsigned int i;
	enum v4l2_buf_type type;
	int ret;

	////  struct v4l2_plane plane;
	log(DEBUG,"%s +\n", __func__);

	for (i = 0; i < n_buffer; i++) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == ioctl(cam_fd, VIDIOC_QBUF, &buf)) {
			perror("cam Fail to ioctl 'VIDIOC_QBUF'");
			exit( EXIT_FAILURE);
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(cam_fd, VIDIOC_STREAMON, &type)) {
		log(DEBUG,"i = %d.\n", i);
		perror("cam_fd Fail to ioctl 'VIDIOC_STREAMON'");
		exit( EXIT_FAILURE);
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = ioctl(fimc0_fd, VIDIOC_STREAMON, &type);
	if (ERR_ON(ret < 0, "fimc0: VIDIOC_STREAMON: %s\n", ERRSTR)) {
		return -errno;
	}
	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int xioctl(int fd, int request, void* argp) {
	int r;
	do {
		r = ioctl(fd, request, argp);
		//log(ERROR,"ret: %d, errno: %d \n", r, errno);//此处 r=0 errno 是其他的错误
	} while (-1 == r && EINTR == errno);

	return r;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int cam_cap_dbuf(int *index) {
	log(DEBUG,"%s +\n", __func__);
	struct v4l2_buffer buf;
	int ret;
	bzero(&buf, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	//buf.index=0;
#if 0
	do {
		ret = xioctl(cam_fd, VIDIOC_DQBUF, &buf);

	} while (-1 == ret && EAGAIN == errno);
#else
	if (-1 == ioctl(cam_fd, VIDIOC_DQBUF, &buf)) {
		perror("cam Fail to ioctl 'VIDIOC_DQBUF'");
		exit( EXIT_FAILURE);
	}
#endif
	cam_buffers[buf.index].bytesused = buf.bytesused;
	log(DEBUG,"%s,Line:%d,bytesused:%d\n", __func__, __LINE__, buf.bytesused);
	*index = buf.index;

	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int cam_cap_qbuf(int index) {
	log(DEBUG,"%s +\n", __func__);
	struct v4l2_buffer buf;

	bzero(&buf, sizeof(buf));

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = index;
	if (-1 == ioctl(cam_fd, VIDIOC_QBUF, &buf)) {
		perror("Fail to ioctl 'VIDIOC_QBUF'");
		exit( EXIT_FAILURE);
	}

	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int fimc0_out_qbuf(int index) {
	int ret;
	struct v4l2_buffer b;
	//// struct v4l2_plane plane;
	log(DEBUG,"%s +\n", __func__);
	/* enqueue buffer to fimc0 output */
	CLEAR(b);

	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	b.memory = V4L2_MEMORY_MMAP;
	//b.memory = V4L2_MEMORY_USERPTR;
	b.index = index;
	//b.m.userptr=(long)cam_buffers[index].start;
	//b.m.userptr = (unsigned long) fimc0_src_buf[index].start;

	//printf("idx : %d  fimc buf start :%lx \n", index, b.m.userptr);
	//b.m.userptr	   = (unsigned long)fimc0_out_buf[index].start;
	//b.length	   = (unsigned long)fimc0_out_buf[index].length;
	//b.bytesused	   = fimc0_out_buf[index].length;

	//printf("fimc0_out_buf:0x%08lx,length:%d,byteused:%d\n",fimc0_out_buf[index].start,  fimc0_out_buf[index].length, fimc0_out_buf[index].bytesused);
	//process_image(fimc0_out_buf[index].start,0);
	ret = ioctl(fimc0_fd, VIDIOC_QBUF, &b);

	if (ERR_ON(ret < 0, "fimc0: VIDIOC_QBUF: %s\n", ERRSTR)) {
		return -errno;
	}
	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int fimc0_out_dbuf(int *index) {
	int ret;
	struct v4l2_buffer b;
	log(DEBUG,"%s +\n", __func__);
	/* enqueue buffer to fimc0 output */
	CLEAR(b);

	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	b.memory = V4L2_MEMORY_MMAP;
	//b.memory = V4L2_MEMORY_USERPTR;
#if 0
	do {
		ret = xioctl(fimc0_fd, VIDIOC_DQBUF, &b);
		log(ERROR,"ret: %d, errno: %d\n",ret,errno);
	}while (-1 == ret && EAGAIN == errno);
#else
	if (-1 == ioctl(fimc0_fd, VIDIOC_DQBUF, &b)) {
		log(ERROR,"errno: %d\n", errno);
		perror("fimc Fail to ioctl 'VIDIOC_DQBUF'");
		exit( EXIT_FAILURE);
	}
#endif
	*index = b.index;
	//if (ERR_ON(ret < 0, "fimc0: VIDIOC_DQBUF: %s\n", ERRSTR)) {
	//	return -errno;
	//}
	log(DEBUG,"%s -\n", __func__);
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
void process_cam_to_fimc0_to_lcd() {
	int index;
	log(DEBUG,"%d +\n", index);
	cam_cap_dbuf(&index);
	memcpy(fimc0_src_buf[index].start, cam_buffers[index].start,
			fimc0_src_buf_length);

	fimc0_out_qbuf(index);

	cam_cap_qbuf(index);

	fimc0_out_dbuf(&index);

	//fimc0_out_dbuf(&index);
	memcpy(fb_buf, fimc0_dst_buf[index].start, fimc0_dst_buf_length);

	log(DEBUG,"%s -,index:%d\n", __func__, index);
}

void memcpySpe(void *fb_buf, void *fimc0_dst_buf, int length)
{
	//test 400*240 at point specified
	#define W 400
	#define H 240
	#define x 400
	#define y 240
	int* src,*dst;
	src=(int*)fimc0_dst_buf;
	dst=(int*)fb_buf+y*800+x;

	int i,j;

	for(i=H;i>0;i--)
	{
		for(j=W;j>0;j--)
		{*dst++=*src++;}
		dst+=400;
		src+=400;
	}

}
/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int mainloop(int cam_fd,lcd & lcddev) {
	int count = 1; //CapNum;
	//clock_t startTime, finishTime;
	//double selectTime, frameTime;
	struct pollfd fds[2];
	//int nfds = 0;
	int index;

	while (count++ > 0) {
		{
			//struct timeval tv;
			int r;
			struct timeval start;
			struct timeval end;
			int time_use = 0;
			gettimeofday(&start, NULL);

			fds[0].events |= POLLIN | POLLPRI;
			fds[0].fd = cam_fd;

			fds[1].events |= POLLIN | POLLPRI | POLLOUT;
			fds[1].fd = fimc0_fd;
			//++nfds;

			r = poll(fds, 1, -1);
			log(DEBUG,"r=%d ,fds[0].revents=%d\n", r, fds[0].revents);
			if (-1 == r) {
				if ( EINTR == errno) {
					continue;
				}

				perror("Fail to select");
				exit( EXIT_FAILURE);
			}
			if (0 == r) {
				log(DEBUG,"t out");
				fprintf( stderr, "select Timeout\n");
				exit( EXIT_FAILURE);
			}

			if (fds[0].revents & POLLIN) {
				//printf("in camfd pollin");
				cam_cap_dbuf(&index);
				memcpy(fimc0_src_buf[index].start, cam_buffers[index].start,
						fimc0_src_buf_length);

				fimc0_out_qbuf(index);

				cam_cap_qbuf(index);

				fimc0_out_dbuf(&index);

				//fimc0_out_dbuf(&index);
				//memcpy(fb_buf, fimc0_dst_buf[index].start,fimc0_dst_buf_length);
				//memcpySpe(fb_buf, fimc0_dst_buf[index].start,fimc0_dst_buf_length);
				lcddev.display(fimc0_dst_buf[index].start);
				gettimeofday(&end, NULL);
				time_use = (end.tv_sec - start.tv_sec) * 1000000
						+ (end.tv_usec - start.tv_usec);
				log(DEBUG,"time_use is %dms\n", time_use / 1000);
			}
			if (fds[1].revents & POLLIN) {
				log(DEBUG,"fimc0 has data to read\n");
				fimc0_out_dbuf(&index);
				memcpy(fb_buf, fimc0_dst_buf[index].start,
						fimc0_dst_buf_length);

			}
			if (fds[1].revents & POLLOUT) {
				log(DEBUG,"fimc0 can be write now\n");
				fimc0_out_qbuf(index);
			}
		}
	}
	return 0;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
void stop_capturing(int cam_fd) {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(cam_fd, VIDIOC_STREAMOFF, &type)) {
		perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
		exit( EXIT_FAILURE);
	}
	return;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
void uninit_camer_device() {
	unsigned int i;

	for (i = 0; i < n_buffer; i++) {
		if (-1 == munmap(fimc0_src_buf[i].start, fimc0_src_buf[i].length)) {
			exit( EXIT_FAILURE);
		}
	}
	if (-1 == munmap(fb_buf, lcd_buf_size)) {
		perror(" Error: framebuffer device munmap() failed.\n");
		exit( EXIT_FAILURE);
	}
	free(fimc0_src_buf);

	return;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
void close_camer_device(int lcd_fd, int cam_fd) {
	if (-1 == close(lcd_fd)) {
		perror("Fail to close lcd_fd");
		exit( EXIT_FAILURE);
	}
	if (-1 == close(cam_fd)) {
		perror("Fail to close cam_fd");
		exit( EXIT_FAILURE);
	}

	return;
}

/***********************************************************
 * Function:       //
 * Description:    //
 * Others:         //
 ***********************************************************/
int main() {
	//open_lcd_device();
	std::string str="/dev/fb0";
	lcd lcddev(str);
	struct display_format dis_f={50,100,300,300};
	lcddev.set_display_format(dis_f);
	open_camera_device();
	init_device();
	start_capturing(cam_fd);
	//pthread_create( &capture_tid, NULL, cam_thread, (void *)NULL );
	//pthread_create(&display_tid,NULL,display_thread,(void *)NULL);
	//char * cambuf=calloc(1,640*480*2);
	while (1) {
		mainloop(cam_fd,lcddev);
		//process_cam_to_fimc0_to_lcd( );
		//sleep( 1 );
	}
	stop_capturing(cam_fd);
	uninit_camer_device();
	close_camer_device(lcd_fd, cam_fd);
	return 0;
}

/************************************** The End Of File **************************************/
