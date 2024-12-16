#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
 
/* 显示屏相关头文件 */
#include <linux/fb.h>
#include <sys/mman.h>

 
typedef struct lcd_color
{
    unsigned char bule;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
} lcd_color;
 
/**
 * 更新屏幕显示内存块信息，颜色格式为RGB8888
*/
void screen_refresh(char *fbp, lcd_color color_buff, long screen_size)
{
    for(int i=0; i < screen_size; i+=4)
    {
        *((lcd_color*)(fbp + i)) = color_buff;
    }
    usleep(1000*2000);
}
 
int main()
{
    int fp = 0;
    int rgb_type = 0;
    long screen_size = 0; 
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;          
    unsigned char *fbp = 0;
 
    fp = open("/dev/fb0", O_RDWR);
 
    if (fp < 0)
    {
        printf("Error : Can not open framebuffer device/n");
        exit(1);
    }
 
    if (ioctl(fp, FBIOGET_FSCREENINFO, &finfo))
    {
        printf("Error reading fixed information/n");
        exit(2);
    }
 
    if (ioctl(fp, FBIOGET_VSCREENINFO, &vinfo))
    {
        printf("Error reading variable information/n");
        exit(3);
    }
 
    /* 打印获取的屏幕信息 */
    printf("The mem is :%d\n", finfo.smem_len);
    printf("The line_length is :%d\n", finfo.line_length);
    printf("The xres is :%d\n", vinfo.xres);
    printf("The yres is :%d\n", vinfo.yres);
    printf("bits_per_pixel is :%d\n", vinfo.bits_per_pixel);
 
    /* 获取RGB的颜色颜色格式，比如RGB8888、RGB656 */
    rgb_type = vinfo.bits_per_pixel / 8;
    /* 屏幕的像素点 */
    screen_size = vinfo.xres * vinfo.yres * rgb_type;
    /* 映射 framebuffer 的缓冲空间，得到一个指向这块空间的指针 */
    fbp =(unsigned char *) mmap (NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    if (fbp == NULL)
    {
       printf ("Error: failed to map framebuffer device to memory./n");
       exit (4);
    }
 
    /* 刷白屏 */
    memset(fbp, 0xff, screen_size);    
    usleep(1000*2000);
 
    /* 我的显示屏是RGBA的，所以颜色格式为32为，注意自己的显示屏信息，对应修改 */
    /* 刷红色 */
    screen_refresh(fbp, (lcd_color){0, 0, 255, 0}, screen_size/8);
	 usleep(1000*2000);
 
    /* 刷绿色 */
    screen_refresh(fbp, (lcd_color){0, 255, 0, 0}, screen_size/5);
	 usleep(1000*2000);
 
    /* 刷蓝色 */
    screen_refresh(fbp, (lcd_color){255, 0, 0, 0}, screen_size/3);
	 usleep(1000*2000);
 
    /* 解除映射 */
    munmap (fbp, screen_size); 
 
    close(fp);
    return 0;
}