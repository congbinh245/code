#!/usr/bin/env bash
set -e

APP_NAME="holo_v2"
MEDIA_DEV="/dev/media0"
VIDEO_DEV="/dev/video0"
DRIVER_KO="/home/ubuntu/driver_holo/xilinx-hologram.ko"

echo "=== 1. Nap Kernel Driver Hologram Subdev ==="
if lsmod | grep -q "xilinx_hologram"; then
    echo "[*] Driver xilinx-hologram da duoc nap truoc do."
else
    if [ -f "$DRIVER_KO" ]; then
        sudo insmod "$DRIVER_KO"
        echo "[+] Nap thanh cong $DRIVER_KO"
    else
        echo "[-] Khong tim thay file $DRIVER_KO! Hay make driver truoc."
        exit 1
    fi
fi

echo "=== 2. Nap Device Tree Overlay qua xmutil ==="
sudo xmutil unloadapp 2>/dev/null || true
sudo xmutil loadapp "$APP_NAME"
sleep 1

echo "=== 3. Kiem tra Driver Binding ==="
if [ -d "/sys/bus/platform/drivers/xilinx-hologram" ]; then
    echo "[+] IP Hologram da duoc bind vao driver:"
    ls -l /sys/bus/platform/drivers/xilinx-hologram | grep a0010000 || true
fi

echo "=== 4. Cau hinh Pipeline bang media-ctl ==="
# 4.1. Reset link va cau hinh sensor IMX219 (1080p RAW10)
media-ctl -d $MEDIA_DEV -r
media-ctl -d $MEDIA_DEV -V '"imx219 6-0010":0 [fmt:SRGGB10_1X10/1920x1080 field:none colorspace:srgb]'

# 4.2. MIPI CSI-2 RX Subsystem
media-ctl -d $MEDIA_DEV -V '"80000000.mipi_csi2_rx_subsystem":0 [fmt:SRGGB10_1X10/1920x1080 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"80000000.mipi_csi2_rx_subsystem":1 [fmt:SRGGB10_1X10/1920x1080 field:none colorspace:srgb]'

# 4.3. Demosaic (RAW10 -> RBG24)
media-ctl -d $MEDIA_DEV -V '"a0020000.v_demosaic":0 [fmt:SRGGB10_1X10/1920x1080 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"a0020000.v_demosaic":1 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'

# 4.4. Gamma LUT
media-ctl -d $MEDIA_DEV -V '"a0080000.v_gamma_lut":0 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"a0080000.v_gamma_lut":1 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'

# 4.5. VPSS CSC
media-ctl -d $MEDIA_DEV -V '"a0000000.v_proc_ss":0 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"a0000000.v_proc_ss":1 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'

# 4.6. VPSS Scaler (Nhan 1080p -> Thu nho thanh 360x360)
media-ctl -d $MEDIA_DEV -V '"a0040000.v_proc_ss":0 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"a0040000.v_proc_ss":1 [fmt:RBG888_1X24/360x360 field:none colorspace:srgb]'

# 4.7. Hologram Subdev (Nhan 360x360 -> Tai cau truc thanh 1080p)
# Ten entity tren media-ctl co dang xilinx-hologram.a0010000
media-ctl -d $MEDIA_DEV -V '"xilinx-hologram.a0010000":0 [fmt:RBG888_1X24/360x360 field:none colorspace:srgb]'
media-ctl -d $MEDIA_DEV -V '"xilinx-hologram.a0010000":1 [fmt:RBG888_1X24/1920x1080 field:none colorspace:srgb]'

echo "=== 5. Thiet lap dinh dang Capture cho $VIDEO_DEV ==="
v4l2-ctl -d $VIDEO_DEV --set-fmt-video=width=1920,height=1080,pixelformat=RGB3,field=none,colorspace=srgb

echo "=== 6. Kiem tra trang thai cuoi ==="
v4l2-ctl -d $VIDEO_DEV -V

echo "[+] Toan bo he thong da san sang capture!"
