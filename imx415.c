#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <linux/fb.h>

#define FMT_NUM_PLANES 1   //出图格式所使用的平面数(BGRX:1, NV12:2)

unsigned int screen_size;
unsigned char * screen_base = NULL;
unsigned int total_size = 0, total_byte = 0;


struct buffer{
    void *start;
    int length;
};

struct v4l2_dev
{
    int fd;
    int sub_fd;
    const char *path;
    const char *name;
    const char *subdev_path;
    const char *out_type;
    enum v4l2_buf_type buf_type;
    int format;
    int width;
    int height;
    unsigned int req_count;
    enum v4l2_memory memory_type;
    struct buffer *buffers;
    unsigned long int timestamp;
    int data_len;
    unsigned char *out_data;
};

struct v4l2_dev imx415 = {                        
    .fd = -1,
    .sub_fd = -1,
    .path = "/dev/video1",
    .name = "imx415",
    .subdev_path = "/dev/v4l-subdev3",
    .out_type = "bgr",
    .buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    .format = V4L2_PIX_FMT_XBGR32,
    .width = 1088,                                      //LCD需要做16单位对齐
    .height = 1080,
    .req_count = 4,
    .memory_type = V4L2_MEMORY_MMAP,
    .buffers = NULL,
    .timestamp = 0,
    .data_len = 0,
    .out_data = NULL,
};

void Close_Device(struct v4l2_dev *dev)
{
    if (dev->buffers) {
        for (unsigned int i = 0; i < dev->req_count; ++i) {
            if (dev->buffers[i].start) {
                munmap(dev->buffers[i].start, dev->buffers[i].length);
            }
        }
        free(dev->buffers);
    }
    if (-1 != dev->fd) {
        close(dev->fd);
    }
    if (-1 != dev->sub_fd) {
        close(dev->sub_fd);
    }
    return;
}

void exit_failure(struct v4l2_dev *dev)
{
    Close_Device(dev);
    exit(EXIT_FAILURE);
}

void Open_dev(struct v4l2_dev *dev)
{
    dev->fd = open(dev->path, O_RDWR | O_CLOEXEC, 0);         //打开video1
    if (dev->fd < 0) {
        printf("Cannot open %s\n\n", dev->path);
        exit_failure(dev);
    }
    printf("Open %s succeed - %d\n\n", dev->path, dev->fd);

    dev->sub_fd = open(dev->subdev_path, O_RDWR|O_CLOEXEC, 0);//打开v4l-subdev3
    if (dev->sub_fd < 0) {
        printf("Cannot open %s\n\n", dev->subdev_path);
        exit_failure(dev);
    }
    printf("Open %s succeed\n\n", dev->subdev_path);
    return;
}

void Get_Capabilities(struct v4l2_dev *dev)
{
    struct v4l2_capability cap;
    if (ioctl(dev->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        printf("VIDIOC_QUERYCAP failed\n");
        return;
    }
    printf("------- VIDIOC_QUERYCAP ----\n");
    printf("  driver: %s\n", cap.driver);
    printf("  card: %s\n", cap.card);
    printf("  bus_info: %s\n", cap.bus_info);
    printf("  version: %d.%d.%d\n",
           (cap.version >> 16) & 0xff,
           (cap.version >> 8) & 0xff,
           (cap.version & 0xff));
    printf("  capabilities: %08X\n", cap.capabilities);

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        printf("        Video Capture\n");
    if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)
        printf("        Video Output\n");
    if (cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)
        printf("        Video Overly\n");
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        printf("        Video Capture Mplane\n");
    if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
        printf("        Video Output Mplane\n");
    if (cap.capabilities & V4L2_CAP_READWRITE)
        printf("        Read / Write\n");
    if (cap.capabilities & V4L2_CAP_STREAMING)
        printf("        Streaming\n");
    printf("\n");
    return;
}


void Set_Format(struct v4l2_dev *dev)
{
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = dev->buf_type;                 
    fmt.fmt.pix_mp.pixelformat = dev->format;
    fmt.fmt.pix_mp.width = dev->width;
    fmt.fmt.pix_mp.height = dev->height;

    //ioctl驱动似乎会默认配置fmt.fmt.pix_mp.num_planes为1，所以当format为mplane时，需手动配置
    //ioctl根据dev->type的格式配置plane_fmt[]
    if (ioctl(dev->fd, VIDIOC_S_FMT, &fmt) < 0) {   
        printf("VIDIOC_S_FMT failed - [%d]!\n", errno);
        exit_failure(dev);
    }

    fmt.fmt.pix_mp.num_planes = FMT_NUM_PLANES;
    
    printf("VIDIOC_S_FMT succeed!\n");

    for(int i = 0; i < fmt.fmt.pix_mp.num_planes; i++)
    {
        total_size += fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
        total_byte += fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
    }

    dev->data_len = total_size;

    printf("width %d, height %d, size %d, bytesperline %d, format %c%c%c%c\n\n",
           fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, dev->data_len,
           total_byte,
           fmt.fmt.pix_mp.pixelformat & 0xFF,
           (fmt.fmt.pix_mp.pixelformat >> 8) & 0xFF,
           (fmt.fmt.pix_mp.pixelformat >> 16) & 0xFF,
           (fmt.fmt.pix_mp.pixelformat >> 24) & 0xFF);
    return;
}



void Require_Buf(struct v4l2_dev *dev)
{
    // 申请缓冲区
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = dev->req_count;
    req.type = dev->buf_type;
    req.memory = dev->memory_type;

    if (ioctl(dev->fd, VIDIOC_REQBUFS, &req) == -1) {
        printf("VIDIOC_REQBUFS failed!\n\n");
        exit_failure(dev);
    }
    if (dev->req_count != req.count) {
        printf("!!! req count = %d\n", req.count);
        dev->req_count = req.count;
    }
    printf("VIDIOC_REQBUFS succeed!\n\n");
    return;
}

void Mmap_Buf(struct v4l2_dev *dev)
{
    unsigned int planes_size = 0;

    dev->buffers = (struct buffer *)calloc(dev->req_count, sizeof(*(dev->buffers)));//动态申请存储帧缓冲信息的buffer
    for (unsigned int i = 0; i < dev->req_count; i++) {                                         
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES]; 
        memset(&buf, 0, sizeof(buf));
        memset(&planes, 0, sizeof(planes));
        buf.type = dev->buf_type;
        buf.memory = dev->memory_type;
        buf.index = i;

        //对于多平面格式要填充以下两个属性
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->buf_type) { 
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        
        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            printf("VIDIOC_QUERYBUF failed!\n\n");
            exit_failure(dev);
        }
        //VIDIOC_QUERYBUF后，buf.length=1 且 planes[1].length=0 怀疑驱动有问题

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->buf_type) {

            dev->buffers[i].length = planes_size; 
            dev->buffers[i].start  = mmap(NULL /* start anywhere */,
                buf.m.planes[0].length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                dev->fd, buf.m.planes[0].m.mem_offset);
        } 
        else { 
            dev->buffers[i].length = buf.length;
            dev->buffers[i].start =  mmap(NULL,
                                         buf.length,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED,
                                         dev->fd,
                                         buf.m.offset);
        }

        if (dev->buffers[i].start == MAP_FAILED) {
            printf("Memory map failed!\n\n");
            exit_failure(dev);
        }
    }
    printf("Memory map succeed!\n\n");
    return;
    
}   

void Queue_Buf(struct v4l2_dev *dev)
{
    for (unsigned int i = 0; i < dev->req_count; i++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        memset(&buf, 0, sizeof(buf));
        memset(&planes, 0, sizeof(planes));
        buf.type = dev->buf_type;
        buf.memory = dev->memory_type;
        buf.index = i;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->buf_type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) < 0) {
            printf("VIDIOC_QBUF failed!\n\n");
            exit_failure(dev);
        }

    }
    printf("VIDIOC_QBUF succeed!\n\n");
    return;
}

void Stream_On(struct v4l2_dev *dev)
{
    enum v4l2_buf_type type = dev->buf_type;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &type) == -1) {
        printf("VIDIOC_STREAMON failed!\n\n");
        exit_failure(dev);
    }
    printf("VIDIOC_STREAMON succeed!\n\n");
    return;
}

void Get_FrameData(struct v4l2_dev *dev, int skip_frame)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[FMT_NUM_PLANES];

    for (int i = 0; i < skip_frame; i++){
        memset(&buf, 0, sizeof(buf));
        buf.type = dev->buf_type;
        buf.memory = dev->memory_type;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->buf_type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (ioctl(dev->fd, VIDIOC_DQBUF, &buf) == -1) {
            printf("VIDIOC_DQBUF failed!\n\n");
            exit_failure(dev);
        }

        dev->out_data = (unsigned char *)dev->buffers[buf.index].start;
        dev->timestamp = buf.timestamp.tv_sec * 1000000 + buf.timestamp.tv_usec;
        printf("image: sequence = %d, timestamp = %lu\n", buf.sequence, dev->timestamp);

        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
            printf("VIDIOC_QBUF failed!\n");
            exit_failure(dev);
        }
    }
    return;
}

void Save_FrameData(const char *filename, unsigned char *file_data, unsigned int len, int is_overwrite)
{
    FILE *fp;
    if (is_overwrite)
        fp = fopen(filename, "wb");
    else
        fp = fopen(filename, "ab");
    if (fp < 0) {
        printf("Open frame data file failed\n\n");
        return;
    }
    if (fwrite(file_data, 1, len, fp) < len) {
        printf("Out of memory!\n");
    }
    fflush(fp);
    fclose(fp);
    printf("Save one frame to %s succeed!\n\n", filename);
    return;
}

void Steam_Off(struct v4l2_dev *dev)
{
    enum v4l2_buf_type type;
    type = dev->buf_type;
    if (ioctl(dev->fd, VIDIOC_STREAMOFF, &type) == -1) {
        printf("VIDIOC_STREAMOFF failed!\n\n");
        exit_failure(dev);
    }
    printf("VIDIOC_STREAMOFF succeed!\n\n");
    return;
}



void Set_Fps(struct v4l2_dev *dev, unsigned int fps)
{
	struct v4l2_subdev_frame_interval frame_int;

    if (fps == 0) return;

	memset(&frame_int, 0x00, sizeof(frame_int));

	frame_int.interval.numerator = 10000;
	frame_int.interval.denominator = fps * 10000;	

	if (ioctl(dev->sub_fd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &frame_int) < 0) 
    {
		perror("VIDIOC_SUBDEV_S_FRAME_INTERVAL fail");
        return;
	}
    printf("VIDIOC_SUBDEV_S_FRAME_INTERVAL [%u fps] OK\n", fps);
    return;
}

void Screen_Init(void)
{
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;
    int framebuff_fd = 0;

    framebuff_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC, 0); 
    if(framebuff_fd < 0){
        perror("open screen fail\n");
    }

    ioctl(framebuff_fd, FBIOGET_VSCREENINFO, &fb_var);
    ioctl(framebuff_fd, FBIOGET_FSCREENINFO, &fb_fix);
    screen_size = fb_fix.line_length * fb_var.yres;

    screen_base = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, framebuff_fd, 0);
    if(MAP_FAILED == screen_base){
        perror("screen mmap fail\n");
        close(framebuff_fd);
        exit(EXIT_FAILURE);
    }

    if(0 > memset(screen_base, 0, screen_size)){
        perror("memset fail");
        exit(EXIT_FAILURE);
    }
    return;
}

void Screen_Show(struct v4l2_dev *dev)
{   
    struct v4l2_buffer buf;
    struct v4l2_plane planes[FMT_NUM_PLANES];
    
    while (1){
        buf.type = dev->buf_type;
        buf.memory = dev->memory_type;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == dev->buf_type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (ioctl(dev->fd, VIDIOC_DQBUF, &buf) == -1) {
            printf("VIDIOC_DQBUF failed!\n");
            exit_failure(dev);
        }

        dev->out_data = (unsigned char *)dev->buffers[buf.index].start;

        for(int i = 0; i < dev->data_len; i++)
        {
            *(screen_base + i) = *(dev->out_data + i);
        }

        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
            printf("VIDIOC_QBUF failed!\n");
            exit_failure(dev);
        }

    }
    return;
} 

int main(int argc, char *argv[])
{
    struct v4l2_dev *dev = &imx415;
	unsigned int fps = 0;
    char name[50] = {0};

    if (argc > 1)
        fps = atoi(argv[1]);
    
    Screen_Init();
    Open_dev(dev);                   // 1 打开摄像头设备
    Get_Capabilities(dev);
    Set_Format(dev);                 // 2 设置出图格式
    Require_Buf(dev);                // 3 申请缓冲区
    Mmap_Buf(dev);                   // 4 内存映射
    Queue_Buf(dev);                  // 5 将缓存帧加入队列
    Set_Fps(dev, fps);               // IMX415驱动不支持VIDIOC_SUBDEV_S_FRAME_INTERVAL
    Stream_On(dev);                  // 6 开启视频流
    
    Get_FrameData(dev, 10);// 7 取一帧数据
    snprintf(name, sizeof(name), "/home/%s.%s", dev->name, dev->out_type);
    Save_FrameData(name, dev->out_data, dev->data_len, 1);

    Screen_Show(dev);                 //LCD显示dev数据流

    Steam_Off(dev);                   // 8 关闭视频流
    Close_Device(dev);                // 9 释放内存关闭文件

    return 0;
}