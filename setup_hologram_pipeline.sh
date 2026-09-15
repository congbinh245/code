#!/bin/bash
###############################################################################
# setup_hologram_pipeline.sh
#
# Tu dong cau hinh media pipeline cho he thong Hologram tren KV260:
#   IMX219 -> CSI-2 RX -> Demosaic -> Gamma LUT -> VPSS CSC -> VPSS Scaler
#           -> Hologram Splitter 4-way -> v_frmbuf_wr -> /dev/videoX
#
# Cach dung:
#   sudo ./setup_hologram_pipeline.sh
#
# Script se TU DO TIM ten entity thuc te qua `media-ctl -p`, khong hardcode
# ten bus I2C hay so video device, vi cac thu nay co the khac nhau giua
# cac lan build / cac board.
###############################################################################

set -e

MEDIA_DEV="${MEDIA_DEV:-/dev/media0}"

echo "==> Doc media graph tu $MEDIA_DEV"
GRAPH="$(media-ctl -d "$MEDIA_DEV" -p)"

if [ -z "$GRAPH" ]; then
    echo "LOI: khong doc duoc media graph. Kiem tra:"
    echo "  - Bitstream + dtbo da nap chua? (xmutil listapps / xmutil loadapp holo_v2)"
    echo "  - $MEDIA_DEV co ton tai khong? (ls /dev/media*)"
    exit 1
fi

# Ham tien ich: tim ten entity day du dua theo tu khoa (grep -m1)
find_entity() {
    local keyword="$1"
    echo "$GRAPH" | grep -oP "(?<=- entity \d: )[^(]*${keyword}[^\(]*" \
        | head -n1 | sed 's/[[:space:]]*$//'
}

echo "==> Dang tim cac entity trong pipeline..."

SENSOR_ENT="$(find_entity 'imx219')"
CSI_ENT="$(find_entity 'mipi_csi2_rx_subsystem')"
DEMOSAIC_ENT="$(find_entity 'v_demosaic')"
GAMMA_ENT="$(find_entity 'v_gamma_lut')"
# Co 2 entity ten "v_proc_ss" (CSC va Scaler) - phan biet bang dia chi trong ten
CSC_ENT="$(echo "$GRAPH" | grep -oP '(?<=- entity \d: )a0000000\.v_proc_ss[^\(]*' | head -n1)"
SCALER_ENT="$(echo "$GRAPH" | grep -oP '(?<=- entity \d: )a0040000\.v_proc_ss[^\(]*' | head -n1)"
HOLO_ENT="$(find_entity 'hologram_splitter_4way')"
FRMBUF_ENT="$(find_entity 'v_frmbuf_wr')"

# In ra de ban tu kiem tra truoc khi chay
echo "  Sensor   : ${SENSOR_ENT:-KHONG TIM THAY}"
echo "  CSI-RX   : ${CSI_ENT:-KHONG TIM THAY}"
echo "  Demosaic : ${DEMOSAIC_ENT:-KHONG TIM THAY}"
echo "  Gamma    : ${GAMMA_ENT:-KHONG TIM THAY}"
echo "  CSC      : ${CSC_ENT:-KHONG TIM THAY}"
echo "  Scaler   : ${SCALER_ENT:-KHONG TIM THAY}"
echo "  Hologram : ${HOLO_ENT:-KHONG TIM THAY}"
echo "  FrmbufWr : ${FRMBUF_ENT:-KHONG TIM THAY}"
echo ""

# Neu bat ky entity nao khong tim thay, dung lai ngay - dung doan mo hinh
for name in SENSOR_ENT CSI_ENT DEMOSAIC_ENT GAMMA_ENT CSC_ENT SCALER_ENT HOLO_ENT FRMBUF_ENT; do
    if [ -z "${!name}" ]; then
        echo "LOI: khong tim thay entity cho $name."
        echo "Chay 'media-ctl -d $MEDIA_DEV -p' thu cong va doi chieu ten trong graph."
        exit 1
    fi
done

echo "==> Bat dau set format cho tung pad (theo dung dataflow)"

set -x

# 0. Sensor IMX219 - dau nguon
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$SENSOR_ENT\":0 [fmt:SRGGB10_1X10/1920x1080]"

# 1. CSI-2 RX subsystem
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$CSI_ENT\":0 [fmt:SRGGB10_1X10/1920x1080]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$CSI_ENT\":1 [fmt:SRGGB10_1X10/1920x1080]"

# 2. Demosaic: Bayer RAW10 -> RBG888
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$DEMOSAIC_ENT\":0 [fmt:SRGGB10_1X10/1920x1080]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$DEMOSAIC_ENT\":1 [fmt:RBG888_1X24/1920x1080]"

# 3. Gamma LUT: giu RBG888
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$GAMMA_ENT\":0 [fmt:RBG888_1X24/1920x1080]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$GAMMA_ENT\":1 [fmt:RBG888_1X24/1920x1080]"

# 4. VPSS CSC: giu RBG888
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$CSC_ENT\":0 [fmt:RBG888_1X24/1920x1080]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$CSC_ENT\":1 [fmt:RBG888_1X24/1920x1080]"

# 5. VPSS Scaler: 1920x1080 -> 360x360
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$SCALER_ENT\":0 [fmt:RBG888_1X24/1920x1080]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$SCALER_ENT\":1 [fmt:RBG888_1X24/360x360]"

# 6. Hologram splitter: 360x360 -> 1920x1080 (canvas ghep 4 goc)
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$HOLO_ENT\":0 [fmt:RBG888_1X24/360x360]"
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$HOLO_ENT\":1 [fmt:RBG888_1X24/1920x1080]"

# 7. Frame buffer writer: nhan 1920x1080, ghi RAM qua HP0
media-ctl -d "$MEDIA_DEV" --set-v4l2 "\"$FRMBUF_ENT\":0 [fmt:RBG888_1X24/1920x1080]"

set +x

echo ""
echo "==> Tim video node tuong ung voi $FRMBUF_ENT"
VIDEO_DEV="$(media-ctl -d "$MEDIA_DEV" -e "$FRMBUF_ENT" 2>/dev/null || true)"

if [ -z "$VIDEO_DEV" ]; then
    echo "CANH BAO: khong tu dong tim duoc video node qua 'media-ctl -e'."
    echo "Kiem tra thu cong bang: v4l2-ctl --list-devices"
    echo "Sau do set format thu cong, vi du:"
    echo "  v4l2-ctl -d /dev/videoX --set-fmt-video=width=1920,height=1080,pixelformat=RGB3"
else
    echo "  -> $VIDEO_DEV"
    echo "==> Set format V4L2 capture tren $VIDEO_DEV"
    set -x
    v4l2-ctl -d "$VIDEO_DEV" --set-fmt-video=width=1920,height=1080,pixelformat=RGB3
    set +x
fi

echo ""
echo "==> HOAN TAT cau hinh pipeline."
echo "Kiem tra lai toan bo topology bang:"
echo "  media-ctl -d $MEDIA_DEV -p"
echo ""
echo "Thu stream 1 frame de xac nhan co du lieu that:"
echo "  v4l2-ctl -d ${VIDEO_DEV:-/dev/videoX} --stream-mmap --stream-count=1 --stream-to=test.raw"
