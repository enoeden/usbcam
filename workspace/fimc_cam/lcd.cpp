/*
 * lcd.cpp
 *
 *  Created on: 2015年6月9日
 *      Author: root
 */
#include "lcd.h"

lcd::lcd(std::string & path)
{
	lcd_path=path;
	if (open_lcd_device()) {perror("!!");};
	if (init_lcd_device()) {perror("!!");};
}
lcd::~lcd(){

	if (-1 == munmap(fb_buf, lcd_buf_size)) {
		perror(" Error: framebuffer device munmap() failed.\n");
		//exit( EXIT_FAILURE);
	}

	if (-1 == close(fd)) {
			perror("Fail to close lcd_fd");
	}
	std::cout<<"out lcd"<<std::endl;

}
int lcd::open_lcd_device()
{
	int lcd_fd;
	if ((lcd_fd = open(lcd::lcd_path.c_str(), O_RDWR | O_NONBLOCK)) < 0) {
		perror("Fail to open");
		//exit( EXIT_FAILURE);
		return -1;
	}
	lcd::fd =lcd_fd;
	log(DEBUG,"open lcd success %d\n", fd);
	return 0;

}

int lcd::init_lcd_device()
{
	static struct fb_var_screeninfo vinfo;
	static struct fb_fix_screeninfo finfo;

	if (-1 == ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Fail to ioctl:FBIOGET_FSCREENINFO\n");
		//exit( EXIT_FAILURE);
		return -1;
	}
	if (-1 == ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("Fail to ioctl:FBIOGET_VSCREENINFO\n");
		//exit( EXIT_FAILURE);
		return -1;
	}
	lcd_buf_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	fbinfo.xres=vinfo.xres;
	fbinfo.yres=vinfo.yres;
	fbinfo.color_bits=vinfo.bits_per_pixel;


	dis_f.x=0;
	dis_f.y=0;
	dis_f.width=fbinfo.xres;
	dis_f.height=fbinfo.yres;

	log(INFO,"vinfo.xres:%d, vinfo.yres:%d, vinfo.bits_per_pixel:%d, lcd_buf_size:%d, finfo.line_length:%d\n",
					vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, lcd_buf_size,
					finfo.line_length);



	vinfo.activate = FB_ACTIVATE_FORCE;
	vinfo.yres_virtual = vinfo.yres;

	int ret = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
	if (ret < 0) {
		log(DEBUG,"ioctl FBIOPUT_VSCREENINFO failed\n");
		return -1;
	}

	//mmap framebuffer
	fb_buf = (char *) mmap(
	NULL, lcd_buf_size,
	PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ( NULL == fb_buf) {
		perror("Fail to mmap fb_buf");
		return -1;
		//exit( EXIT_FAILURE);
	}
	ret = ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK);
	if (ret < 0) {
		log(ERROR,"ioctl FBIOBLANK failed\n");
		return -1;
	}
	log(INFO,"fb address %lx\n", (long int)fb_buf);
	return 0;
}

void lcd::get_info(){
	std::cout<<"lcd_file_path: "<<lcd_path<<std::endl;
	std::cout<<"lcd_file_description: "<<fd<<std::endl;
	std::cout<<"lcd_file_path"<<lcd_path<<std::endl;
	std::cout<<"lcd_xres: "<<fbinfo.xres<<"lcd_yres: "<<fbinfo.yres<<"lcd_bits: "<<fbinfo.color_bits<<std::endl;
	std::cout<<"lcd_buf_address: 0x"<<std::hex<<fb_buf<<std::endl;
}

/*  15-6-9 19:21 add get&set_display_format() and display() untested */

struct display_format lcd::get_display_format(){
		return dis_f;
}
int lcd::set_display_format(struct display_format f){
		if (f.x>=0 &&f.x<=fbinfo.xres) dis_f.x=f.x;
		else dis_f.x=0;
		if (f.y>=0 &&f.y<=fbinfo.yres) dis_f.y=f.y;
		else dis_f.y=0;

		if ((f.height<=0)||(f.width<=0)) return -1;

		if ((f.x+f.width)>=0 &&(f.x+f.width)<fbinfo.xres) dis_f.width=f.width;
		else dis_f.width=fbinfo.xres-f.x;
		if ((f.y+f.height)>=0 &&(f.y+f.height)<fbinfo.yres) dis_f.height=f.height;
		else dis_f.height=fbinfo.yres-f.y;
		return 0;
}


void lcd::display(void * fb_src_buf)
{
	//test 400*240 at point specified
	//#define W 400
	//#define H 240
	///#define x 400
	//#define y 240
	fb_src_buf=(int *)fb_src_buf;
	fb_buf=(int*)fb_buf;

	int* src,*dst;
	src=fb_src_buf;
	dst=fb_buf+dis_f.y*fbinfo.xres+dis_f.x;

	int i,j;

	for(i=dis_f.height;i>0;i--)
	{
		for(j=dis_f.width;j>0;j--)
		{*dst++=*src++;}
		dst+=fbinfo.xres-dis_f.width;
		src+=fbinfo.xres-dis_f.width;
	}

}
