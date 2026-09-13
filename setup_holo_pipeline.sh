#!/bin/bash
set -e

echo "=== 1. Reset topology media0 ==="
media-ctl -d /dev/media0 -r

echo "=== 2. Cấu hình Camera IMX219 (1080p Raw Bayer 10-bit) ==="
media-ctl -d /dev/media0 --set-v4l2 '"imx219 6-0010":0 [fmt:SRGGB10_1X10/1920x1080]'

echo "=== 3. Cấu hình MIPI CSI-2 Rx Subsystem (0x80000000) ==="
media-ctl -d /dev/media0 --set-v4l2 '"80000000.mipi_csi2_rx_subsystem":0 [fmt:SRGGB10_1X10/1920x1080 field:none]'
media-ctl -d /dev/media0 --set-v4l2 '"80000000.mipi_csi2_rx_subsystem":1 [fmt:SRGGB10_1X10/1920x1080 field:none]'

echo "=== 4. Cấu hình Demosaic (0xA0020000) ==="
media-ctl -d /dev/media0 --set-v4l2 '"a0020000.v_demosaic":0 [fmt:SRGGB10_1X10/1920x1080 field:none]'
media-ctl -d /dev/media0 --set-v4l2 '"a0020000.v_demosaic":1 [fmt:RBG888_1X24/1920x1080 field:none]'

echo "=== 5. Cấu hình Gamma LUT (0xA0080000) ==="
media-ctl -d /dev/media0 --set-v4l2 '"a0080000.v_gamma_lut":0 [fmt:RBG888_1X24/1920x1080 field:none]'
media-ctl -d /dev/media0 --set-v4l2 '"a0080000.v_gamma_lut":1 [fmt:RBG888_1X24/1920x1080 field:none]'

# Tìm subdev node của gamma lut để set gain
GAMMA_DEV=$(media-ctl -d /dev/media0 -p | grep -B 1 "a0080000.v_gamma_lut" | grep "device node" | awk '{print $4}')
if [ -n "$GAMMA_DEV" ]; then
    yavta --no-query -w '0x0098c9c1 10' "$GAMMA_DEV"
    yavta --no-query -w '0x0098c9c2 10' "$GAMMA_DEV"
    yavta --no-query -w '0x0098c9c3 10' "$GAMMA_DEV"
fi

echo "=== 6. Cấu hình VPSS CSC (0xA0000000) ==="
media-ctl -d /dev/media0 --set-v4l2 '"a0000000.v_proc_ss_csc":0 [fmt:RBG888_1X24/1920x1080 field:none]'
media-ctl -d /dev/media0 --set-v4l2 '"a0000000.v_proc_ss_csc":1 [fmt:RBG888_1X24/1920x1080 field:none]'

echo "=== 7. Cấu hình VPSS Scaler (0xA0040000): Thu nhỏ 1080p -> 360x360 ==="
media-ctl -d /dev/media0 --set-v4l2 '"a0040000.v_proc_ss_scaler":0 [fmt:RBG888_1X24/1920x1080 field:none]'
media-ctl -d /dev/media0 --set-v4l2 '"a0040000.v_proc_ss_scaler":1 [fmt:RBG888_1X24/360x360 field:none]'

SCALER_DEV=$(media-ctl -d /dev/media0 -p | grep -B 1 "a0040000.v_proc_ss_scaler" | grep "device node" | awk '{print $4}')
if [ -n "$SCALER_DEV" ]; then
    yavta -w '0x0098c9a1 90' "$SCALER_DEV" || true
    yavta -w '0x0098c9a2 50' "$SCALER_DEV" || true
    yavta -w '0x0098c9a3 35' "$SCALER_DEV" || true
    yavta -w '0x0098c9a4 24' "$SCALER_DEV" || true
    yavta -w '0x0098c9a5 40' "$SCALER_DEV" || true
fi

echo "=== 8. Cấu hình Cảm biến ==="
v4l2-ctl --set-ctrl=analogue_gain=120 || true
v4l2-ctl --set-ctrl=digital_gain=400 || true

echo "Pipeline sẵn sàng!"
