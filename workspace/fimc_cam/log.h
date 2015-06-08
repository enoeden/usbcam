/*
 * log.h
 *
 *  Created on: 2015年6月8日
 *      Author: root
 */

#ifndef LOG_H_
#define LOG_H_

#include  <stdio.h>

#define DEBUG 0
#define INFO 1
#define ERROR 2

//#define LEVEL 1

#define log(level,...)  do { if ( LEVEL <= level ) printf(__VA_ARGS__);} while(0)




#endif /* LOG_H_ */
