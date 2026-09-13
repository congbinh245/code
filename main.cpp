#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <cstring>
#include <vector>

// Cập nhật chuẩn theo Address Editor Vivado của bạn
#define HOLOGRAM_BASE_ADDR   0xA0010000  
#define MAP_SIZE             0x10000

#define AP_CTRL_OFFSET       0x00
#define AP_START             (1 << 0)
#define AUTO_RESTART         (1 << 7)

void start_hologram_core(int mem_fd) {
    void* virt_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, HOLOGRAM_BASE_ADDR);
    if (virt_base == MAP_FAILED) {
        perror("Lỗi mmap cho Hologram IP");
        return;
    }

    volatile uint32_t* ap_ctrl = (volatile uint32_t*)((uint8_t*)virt_base + AP_CTRL_OFFSET);

    // Kích hoạt auto-restart và ap_start (0x81)
    *ap_ctrl = AUTO_RESTART | AP_START;
    std::cout << ">> Khởi động Hologram IP tại 0x" << std::hex << HOLOGRAM_BASE_ADDR 
              << " | Control Reg: 0x" << *ap_ctrl << std::dec << std::endl;

    munmap(virt_base, MAP_SIZE);
}

int main() {
    // 1. Kích hoạt IP Hologram qua AXI-Lite
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Không thể mở /dev/mem (Chạy với quyền sudo)");
        return -1;
    }
    start_hologram_core(mem_fd);
    close(mem_fd);

    // 2. Mở V4L2 Device Node từ Frame Buffer Write
    int video_fd = open("/dev/video0", O_RDWR);
    if (video_fd < 0) {
        perror("Không thể mở /dev/video0");
        return -1;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = 1920;
    fmt.fmt.pix_mp.height = 1080;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_BGR24;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;

    if (ioctl(video_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Lỗi VIDIOC_S_FMT");
        close(video_fd);
        return -1;
    }

    // 3. Khởi tạo Buffer DMA
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(video_fd, VIDIOC_REQBUFS, &req);

    struct Buffer { void* start; size_t length; };
    std::vector<Buffer> buffers(req.count);

    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[1];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = 1;

        ioctl(video_fd, VIDIOC_QUERYBUF, &buf);
        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                video_fd, buf.m.planes[0].m.mem_offset);
        ioctl(video_fd, VIDIOC_QBUF, &buf);
    }

    // 4. Bật Streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Lỗi VIDIOC_STREAMON");
        return -1;
    }
    std::cout << ">> Pipeline Hologram 1080p đang chạy ổn định!" << std::endl;

    // Lấy thử 10 frame mẫu
    for (int i = 0; i < 10; i++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[1];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length = 1;

        ioctl(video_fd, VIDIOC_DQBUF, &buf);
        std::cout << "-> Đã nhận Frame " << i << " [1920x1080 Hologram Frame]" << std::endl;
        ioctl(video_fd, VIDIOC_QBUF, &buf);
    }

    // Tắt luồng
    ioctl(video_fd, VIDIOC_STREAMOFF, &type);
    for (auto& b : buffers) munmap(b.start, b.length);
    close(video_fd);

    return 0;
}
