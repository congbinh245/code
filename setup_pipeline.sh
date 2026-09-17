#!/bin/bash
MEDIA="/dev/media0"
VIDEO="/dev/video0"

# 1. Pipeline Sensor -> CSC (1080p)
media-ctl -d $MEDIA -V '"imx219 6-0010":0 [fmt:SRGGB10_1X10/1920x1080]'
media-ctl -d $MEDIA -V '"80000000.mipi_csi2_rx_subsystem":0 [fmt:SRGGB10_1X10/1920x1080]'
media-ctl -d $MEDIA -V '"80000000.mipi_csi2_rx_subsystem":1 [fmt:SRGGB10_1X10/1920x1080]'
media-ctl -d $MEDIA -V '"a0020000.v_demosaic":0 [fmt:SRGGB10_1X10/1920x1080]'
media-ctl -d $MEDIA -V '"a0020000.v_demosaic":1 [fmt:RBG888_1X24/1920x1080]'
media-ctl -d $MEDIA -V '"a0080000.v_gamma_lut":0 [fmt:RBG888_1X24/1920x1080]'
media-ctl -d $MEDIA -V '"a0080000.v_gamma_lut":1 [fmt:RBG888_1X24/1920x1080]'
media-ctl -d $MEDIA -V '"a0000000.v_proc_ss":0 [fmt:RBG888_1X24/1920x1080]'
media-ctl -d $MEDIA -V '"a0000000.v_proc_ss":1 [fmt:RBG888_1X24/1920x1080]'

# 2. Scaler Pad 0 & 1 đặt định dạng 1080p
media-ctl -d $MEDIA -V '"a0040000.v_proc_ss":0 [fmt:RBG888_1X24/1920x1080]'
media-ctl -d $MEDIA -V '"a0040000.v_proc_ss":1 [fmt:RBG888_1X24/1920x1080]'

# 3. Đặt node video 1080p
v4l2-ctl -d $VIDEO -v width=1920,height=1080,pixelformat='RGB3'
