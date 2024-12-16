#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
 
/* ��ʾ�����ͷ�ļ� */
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
 * ������Ļ��ʾ�ڴ����Ϣ����ɫ��ʽΪRGB8888
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
 
    /* ��ӡ��ȡ����Ļ��Ϣ */
    printf("The mem is :%d\n", finfo.smem_len);
    printf("The line_length is :%d\n", finfo.line_length);
    printf("The xres is :%d\n", vinfo.xres);
    printf("The yres is :%d\n", vinfo.yres);
    printf("bits_per_pixel is :%d\n", vinfo.bits_per_pixel);
 
    /* ��ȡRGB����ɫ��ɫ��ʽ������RGB8888��RGB656 */
    rgb_type = vinfo.bits_per_pixel / 8;
    /* ��Ļ�����ص� */
    screen_size = vinfo.xres * vinfo.yres * rgb_type;
    /* ӳ�� framebuffer �Ļ���ռ䣬�õ�һ��ָ�����ռ��ָ�� */
    fbp =(unsigned char *) mmap (NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    if (fbp == NULL)
    {
       printf ("Error: failed to map framebuffer device to memory./n");
       exit (4);
    }
 
    /* ˢ���� */
    memset(fbp, 0xff, screen_size);    
    usleep(1000*2000);
 
    /* �ҵ���ʾ����RGBA�ģ�������ɫ��ʽΪ32Ϊ��ע���Լ�����ʾ����Ϣ����Ӧ�޸� */
    /* ˢ��ɫ */
    screen_refresh(fbp, (lcd_color){0, 0, 255, 0}, screen_size/8);
	 usleep(1000*2000);
 
    /* ˢ��ɫ */
    screen_refresh(fbp, (lcd_color){0, 255, 0, 0}, screen_size/5);
	 usleep(1000*2000);
 
    /* ˢ��ɫ */
    screen_refresh(fbp, (lcd_color){255, 0, 0, 0}, screen_size/3);
	 usleep(1000*2000);
 
    /* ���ӳ�� */
    munmap (fbp, screen_size); 
 
    close(fp);
    return 0;
}