
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C"{
#endif

#define VIDEO_WIDTH                 (1920)
#define VIDEO_HEIGHT                (1080)
#define VIDEO_FORMAT                (V4L2_PIX_FMT_YUYV)

typedef struct {
    size_t length;
    void * addr;
} camera_buffer_t, * camera_buffer_p;

#define V4L2_REQ_BUFFER_COUNT       (4)
typedef struct {
    int fd;
    int inited;

    struct v4l2_capability v4l2_cap;

    struct v4l2_fmtdesc v4l2_fmtdesc;
    struct v4l2_format v4l2_fmt, v4l2_fmtack;

    camera_buffer_t camera_buffer[V4L2_REQ_BUFFER_COUNT];
    struct v4l2_requestbuffers v4l2_reqbuf;
    struct v4l2_buffer v4l2_buf;

} camera_t, * camera_p;

typedef enum {
    ret_ok                = 0,
    ret_fail              = 1,
    ret_param_illegal     = 2,
    ret_not_inited        = 3,
    ret_not_started       = 4,
} ret_t;

ret_t camera_init(camera_p camera, const char * camera_path, unsigned int width, unsigned height)
{
    if(!camera || !camera_path) {
        return ret_param_illegal;
    }

    camera->inited = 0;

    if(-1 == (camera->fd = open(camera_path, O_RDWR))) {
        printf("camera_init open error : %d !\n", camera->fd);
        return ret_fail;
    }

#if 0
    /* get camera driver infomation */
	if(-1 == ioctl(camera->fd, VIDIOC_QUERYCAP, camera->v4l2_cap)) {
        printf("camera_init ioctl VIDIOC_QUERYCAP error \n");
        goto close_label;
	}

    printf(" ************ Capability Informations ************\n");
    printf(" driver: %s \n", camera->v4l2_cap.driver);
    printf(" card: %s \n", camera->v4l2_cap.card);
    printf(" bus_info: %s \n", camera->v4l2_cap.bus_info);
    printf(" version: %08X \n", camera->v4l2_cap.version);
    printf(" capabilities: %08X \n", camera->v4l2_cap.capabilities);

    /* set camera v4l2 format */
    memset(&camera->v4l2_fmt, 0, sizeof(camera->v4l2_fmt));
    camera->v4l2_fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camera->v4l2_fmt.fmt.pix.width       = width;
    camera->v4l2_fmt.fmt.pix.height      = height;
    camera->v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    camera->v4l2_fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    if (ioctl(camera->fd, VIDIOC_S_FMT, &camera->v4l2_fmt) < 0) {
        printf("camera_init ioctl VIDIOC_S_FMT error \n");
        goto close_label;
    }

    /* get camera video format */
    if (ioctl(camera->fd, VIDIOC_G_FMT, &camera->v4l2_fmtack) < 0) {
        printf("camera_init ioctl VIDIOC_G_FMT error \n");
        goto close_label;
    }

    char fmt_str[8] = {0};
    memcpy(fmt_str, &camera->v4l2_fmt.fmt.pix.pixelformat, 4);

    printf(" ************ Stream Format Informations ************\n");
    printf(" type: %d \n", camera->v4l2_fmtack.type);
    printf(" width: %d \n", camera->v4l2_fmtack.fmt.pix.width);
    printf(" height: %d \n", camera->v4l2_fmtack.fmt.pix.height);
    printf(" pixelformat: %s \n", fmt_str);
    printf(" field: %d \n", camera->v4l2_fmtack.fmt.pix.field);
    printf(" bytesperline: %d \n", camera->v4l2_fmtack.fmt.pix.bytesperline);
    printf(" sizeimage: %d \n", camera->v4l2_fmtack.fmt.pix.sizeimage);
    printf(" colorspace: %d \n", camera->v4l2_fmtack.fmt.pix.colorspace);
    printf(" priv: %d \n", camera->v4l2_fmtack.fmt.pix.priv);
    printf(" raw_date: %s \n", camera->v4l2_fmtack.fmt.raw_data);
#endif 

    /* alloc camera v4l2 request buffers */
    camera->v4l2_reqbuf.count = sizeof(camera->camera_buffer)/sizeof(camera->camera_buffer[0]);
    camera->v4l2_reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camera->v4l2_reqbuf.memory = V4L2_MEMORY_MMAP;

    if(ioctl(camera->fd , VIDIOC_REQBUFS, &camera->v4l2_reqbuf) < 0) {
        printf("camera_init ioctl VIDIOC_REQBUFS error \n");
        goto close_label;
    }

    memset(camera->camera_buffer, 0, sizeof(camera->camera_buffer));

    for (int i = 0; i < camera->v4l2_reqbuf.count; i++) {

        camera->v4l2_buf.index = i;
        camera->v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        camera->v4l2_buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(camera->fd, VIDIOC_QUERYBUF, &camera->v4l2_buf) < 0) {
            printf("camera_init ioctl VIDIOC_QUERYBUF error \n");
            goto munmap_label;
        }

        // mmap buffer
        camera->camera_buffer[i].length = camera->v4l2_buf.length;
        camera->camera_buffer[i].addr = (char *) mmap(0, camera->v4l2_buf.length, PROT_READ  |PROT_WRITE, MAP_SHARED, camera->fd, camera->v4l2_buf.m.offset);
        if (MAP_FAILED == camera->camera_buffer[i].addr) {
            printf("camera_init mmap error : %dth \n", i);
            goto munmap_label;
        }
    
        // Queen buffer
        if (ioctl(camera->fd , VIDIOC_QBUF, &camera->v4l2_buf) < 0) {
            printf("camera_init ioctl VIDIOC_QBUF error \n");
            goto munmap_label;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMON, &type) < 0) {
        printf("camera_init ioctl VIDIOC_STREAMON error \n");
        goto munmap_label;
    }

    camera->inited = 1;
    return ret_ok;

munmap_label:
    for(int i = 0; i < camera->v4l2_reqbuf.count; i++) {
        if(MAP_FAILED != camera->camera_buffer[i].addr && 0 != camera->camera_buffer[i].addr) {
            munmap(camera->camera_buffer[i].addr, camera->camera_buffer[i].length);
        }
    }

close_label:
    close(camera->fd);
    camera->inited = 0;
    return ret_fail;
}

ret_t camera_capture(camera_p camera, void ** buffer, unsigned int * length)
{
    if(!camera) {
        return ret_param_illegal;
    }

    if(!camera->inited) {
        return ret_not_inited;
    }

    if (ioctl(camera->fd, VIDIOC_DQBUF, &camera->v4l2_buf) < 0) {
        printf("camera_capture ioctl VIDIOC_DQBUF error \n");
        return ret_fail;
    }

    *buffer = camera->camera_buffer[camera->v4l2_buf.index].addr;
    *length = camera->camera_buffer[camera->v4l2_buf.index].length;

    return ret_ok;
}

ret_t camera_stop(camera_p camera)
{
    if(!camera) {
        return ret_param_illegal;
    }

    if(!camera->inited) {
        return ret_not_inited;
    }

    if (ioctl(camera->fd, VIDIOC_QBUF, &camera->v4l2_buf) < 0) {
        printf("camera_stop ioctl VIDIOC_QBUF error \n");
        return ret_fail;
    }

    return ret_ok;
}


void camera_uninit(camera_p camera)
{
    if(!camera) {
        return;
    }

    if(!camera->inited) {
        return;
    }

    camera->inited = 0;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(camera->fd, VIDIOC_STREAMOFF, &type);

    for(int i = 0; i < camera->v4l2_reqbuf.count; i++) {
        if(MAP_FAILED != camera->camera_buffer[i].addr && 0 != camera->camera_buffer[i].addr) {
            munmap(camera->camera_buffer[i].addr, camera->camera_buffer[i].length);
        }
    }

    if(camera->fd > 0) {
        close(camera->fd);
    }
}

void main() 
{
    camera_t camera; 
    ret_t ret = camera_init(&camera, "/dev/video0", 1920, 1080);
    if(ret_ok != ret) {
        printf("camera_init error : %d !\n", ret);
        return;
    }

    void * buffer;
    unsigned int length;

    while(1) {
        ret = camera_capture(&camera, &buffer, &length);
        if(ret_ok != ret) {
            printf("camera_capture error : %d !\n", ret);
            break;
        }
		
        // do something, buffer, length;

        ret = camera_stop(&camera);
        if(ret_ok != ret) {
            printf("camera_stop_capture error : %d !\n", ret);
            break;
        }
    }

    camera_uninit(&camera);
}


#ifdef __cplusplus
}
#endif