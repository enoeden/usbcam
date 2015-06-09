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
