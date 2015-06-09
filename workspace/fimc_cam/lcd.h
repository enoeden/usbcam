/*
 * lcd.h
 *
 *  Created on: 2015年6月9日
 *      Author: root
 */

#ifndef LCD_H_
#define LCD_H_
#include <iostream>
#include <fcntl.h>//open ioctl
#include <linux/fb.h>
#include <sys/mman.h>//mmap
#include <sys/ioctl.h>


#include "log.h"
#define LEVEL 1

struct fb_info{
	unsigned long xres;
	unsigned long yres;
	unsigned long color_bits;

};

class lcd{
public:
	lcd(std::string &);
	~lcd();
	void get_info();

private:
	int open_lcd_device();
	int init_lcd_device();

	std::string lcd_path;
	int fd;
	int lcd_buf_size;
	void * fb_buf;
	struct fb_info fbinfo;

};

/*  15-6-9 add v1
 * int main(){
    std::string path1="/dev/fb3";
    lcd lcd1(path1);
    lcd1.get_info();}
 * */

#endif /* LCD_H_ */
