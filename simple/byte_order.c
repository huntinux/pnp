/*************************************************************************
	> File Name: byte_order.c
	> Author: 
	> Mail: 
	> Created Time: 2017年05月05日 星期五 21时46分07秒
 ************************************************************************/

#include <stdio.h>
#include <stdint.h>

union order {
	uint8_t  x;
	uint16_t y;		
};

int main()
{
	union order o;
	o.y = 0x0102;	
	if(o.x == 0x1)  printf("big endian\n");
	else printf("little endian\n");
	return 0;
}
