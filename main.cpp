#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <fstream>

#define ADDR_SCALER      0xA0040000
#define ADDR_HOLO        0xA0010000
#define ADDR_FRMBUF_WR   0xA0030000
#define FRAME_BUFFER_PHY 0x70000000 // Địa chỉ vật lý DDR
#define FRAME_SIZE       (1920 * 1080 * 3)

volatile uint32_t* map_hw(int fd, off_t target) {
    void* map_base = mmap(0, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~0xFFFF);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return (volatile uint32_t*)((uint8_t*)map_base + (target & 0xFFFF));
}

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Cannot open /dev/mem (Need sudo)");
        return -1;
    }

    // 1. Ánh xạ các IP Core
    volatile uint32_t* vpss_scaler = map_hw(fd, ADDR_SCALER);
    volatile uint32_t* holo        = map_hw(fd, ADDR_HOLO);
    volatile uint32_t* frmbuf      = map_hw(fd, ADDR_FRMBUF_WR);

    // 2. Ánh xạ bộ nhớ đệm DDR FrameBuffer vào Userspace để đọc ảnh
    uint8_t* frame_ptr = (uint8_t*)mmap(0, FRAME_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FRAME_BUFFER_PHY);
    if (frame_ptr == MAP_FAILED) {
        perror("mmap FrameBuffer failed");
        return -1;
    }

    std::cout << "[+] Configuring VPSS Scaler (1080p -> 360x360)..." << std::endl;
    // Cấu hình thanh ghi VPSS Scaler AXI-Lite
    vpss_scaler[0x10 / 4] = 1920; // Input Width
    vpss_scaler[0x18 / 4] = 1080; // Input Height
    vpss_scaler[0x20 / 4] = 360;  // Output Width
    vpss_scaler[0x28 / 4] = 360;  // Output Height
    vpss_scaler[0x00 / 4] = 0x81; // Auto-restart + Start

    std::cout << "[+] Starting Hologram Splitter Core..." << std::endl;
    holo[0x00 / 4] = 0x81; // Auto-restart + Start

    std::cout << "[+] Configuring FrameBuffer Write Engine (1920x1080 RGB888)..." << std::endl;
    frmbuf[0x10 / 4] = 1920;             // Width
    frmbuf[0x18 / 4] = 1080;             // Height
    frmbuf[0x20 / 4] = 1920 * 3;         // Stride (Bytes per line)
    frmbuf[0x28 / 4] = 20;               // Video Format ID: RGB888
    frmbuf[0x30 / 4] = FRAME_BUFFER_PHY; // Buffer 0 Base Address (Lower 32-bit)
    frmbuf[0x34 / 4] = 0x00000000;       // Buffer 0 Base Address (Upper 32-bit)
    frmbuf[0x00 / 4] = 0x81;             // ap_start + auto_restart

    std::cout << "[+] Hardware Streaming active. Capturing 1 frame..." << std::endl;
    usleep(100000); // Đợi 100ms cho phần cứng ghi ít nhất 1 frame đầy đủ

    // Lưu frame ra file nhị phân raw để kiểm tra kết quả
    std::ofstream out("frame_hologram_1080p.raw", std::ios::binary);
    out.write((char*)frame_ptr, FRAME_SIZE);
    out.close();
    std::cout << "[+] Saved frame_hologram_1080p.raw (" << FRAME_SIZE << " bytes) successfully!" << std::endl;

    close(fd);
    return 0;
}
