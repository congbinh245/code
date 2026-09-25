// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>

#include <media/v4l2-async.h>
#include <media/v4l2-subdev.h>

#define DRV_NAME "xilinx-hologram"

/* AXI4-Lite Control Registers cho HLS IP */
#define HOLO_AP_CTRL         (0x00)
#define HOLO_START           BIT(0)
#define HOLO_AUTO_RESTART    BIT(7)
#define HOLO_STREAM_ON       (HOLO_AUTO_RESTART | HOLO_START)
#define HOLO_STREAM_OFF      (0x00)

/* Định nghĩa trạng thái Reset GPIO */
#define HOLO_RESET_DEASSERT  (0)
#define HOLO_RESET_ASSERT    (1)

struct holo_dev {
    struct v4l2_subdev subdev;
    struct media_pad pads[2];
    struct v4l2_mbus_framefmt formats[2];
    void __iomem *base;
    struct clk *clk;
    struct gpio_desc *rst_gpio;
};

static inline struct holo_dev *to_holo_dev(struct v4l2_subdev *sd) {
    return container_of(sd, struct holo_dev, subdev);
}

/* 1. Kích hoạt và dừng luồng theo chuẩn V4L2 */
static int holo_s_stream(struct v4l2_subdev *sd, int enable) {
    struct holo_dev *holo = to_holo_dev(sd);

    if (!enable) {
        /* Dừng IP */
        iowrite32(HOLO_STREAM_OFF, holo->base + HOLO_AP_CTRL);
        
        /* Reset chu kỳ để xả sạch FIFO nội bộ */
        if (holo->rst_gpio) {
            gpiod_set_value_cansleep(holo->rst_gpio, HOLO_RESET_ASSERT);
            udelay(10);
            gpiod_set_value_cansleep(holo->rst_gpio, HOLO_RESET_DEASSERT);
        }
        
        if (holo->clk)
            clk_disable(holo->clk);

        dev_info(sd->dev, "Hologram IP: Stream Stopped & Reset cycled\n");
        return 0;
    }

    if (holo->clk)
        clk_enable(holo->clk);

    /* Kích hoạt Auto-restart và Start IP */
    iowrite32(HOLO_STREAM_ON, holo->base + HOLO_AP_CTRL);
    dev_info(sd->dev, "Hologram IP: Started (CTRL = 0x%08x)\n",
             ioread32(holo->base + HOLO_AP_CTRL));

    return 0;
}

/* 2. Hàm lấy pad format tương thích Linux 5.15 */
static struct v4l2_mbus_framefmt *
__holo_get_pad_format(struct holo_dev *holo,
                      struct v4l2_subdev_state *sd_state,
                      unsigned int pad, u32 which)
{
    switch (which) {
    case V4L2_SUBDEV_FORMAT_TRY:
        return v4l2_subdev_get_try_format(&holo->subdev, sd_state, pad);
    case V4L2_SUBDEV_FORMAT_ACTIVE:
        return &holo->formats[pad];
    default:
        return NULL;
    }
}

static int holo_get_fmt(struct v4l2_subdev *sd,
                        struct v4l2_subdev_state *sd_state,
                        struct v4l2_subdev_format *fmt) {
    struct holo_dev *holo = to_holo_dev(sd);
    struct v4l2_mbus_framefmt *get_fmt;

    if (fmt->pad >= 2)
        return -EINVAL;

    get_fmt = __holo_get_pad_format(holo, sd_state, fmt->pad, fmt->which);
    if (!get_fmt)
        return -EINVAL;

    fmt->format = *get_fmt;
    return 0;
}

static int holo_set_fmt(struct v4l2_subdev *sd,
                        struct v4l2_subdev_state *sd_state,
                        struct v4l2_subdev_format *fmt) {
    struct holo_dev *holo = to_holo_dev(sd);
    struct v4l2_mbus_framefmt *__format;

    if (fmt->pad >= 2)
        return -EINVAL;

    __format = __holo_get_pad_format(holo, sd_state, fmt->pad, fmt->which);
    if (!__format)
        return -EINVAL;

    *__format = fmt->format;

    /* Khóa format RBG888_1X24 */
    __format->code = MEDIA_BUS_FMT_RBG888_1X24;
    __format->field = V4L2_FIELD_NONE;
    __format->colorspace = V4L2_COLORSPACE_SRGB;

    /* Độ phân giải cổng */
    if (fmt->pad == 0) {
        __format->width = 360;
        __format->height = 360;
    } else {
        __format->width = 1920;
        __format->height = 1080;
    }

    fmt->format = *__format;
    return 0;
}

static const struct v4l2_subdev_video_ops holo_video_ops = {
    .s_stream = holo_s_stream,
};

static const struct v4l2_subdev_pad_ops holo_pad_ops = {
    .get_fmt = holo_get_fmt,
    .set_fmt = holo_set_fmt,
};

static const struct v4l2_subdev_ops holo_ops = {
    .video = &holo_video_ops,
    .pad = &holo_pad_ops,
};

static const struct media_entity_operations holo_media_ops = {
    .link_validate = v4l2_subdev_link_validate,
};

static int holo_probe(struct platform_device *pdev) {
    struct holo_dev *holo;
    struct resource *res;
    int ret;

    holo = devm_kzalloc(&pdev->dev, sizeof(*holo), GFP_KERNEL);
    if (!holo)
        return -ENOMEM;

    /* 1. Map địa chỉ AXI-Lite */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    holo->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(holo->base))
        return PTR_ERR(holo->base);

    /* 2. Quản lý Clock */
    holo->clk = devm_clk_get_optional(&pdev->dev, "ap_clk");
    if (IS_ERR(holo->clk))
        return PTR_ERR(holo->clk);
    if (holo->clk)
        clk_prepare(holo->clk);

    /* 3. Quản lý GPIO Reset (Active-Low trên phần cứng) */
    holo->rst_gpio = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(holo->rst_gpio)) {
        dev_err(&pdev->dev, "Failed to get reset GPIO\n");
        return PTR_ERR(holo->rst_gpio);
    }

    if (holo->rst_gpio) {
        udelay(100);
        gpiod_set_value_cansleep(holo->rst_gpio, HOLO_RESET_DEASSERT);
        dev_info(&pdev->dev, "Hologram IP: Hardware Reset Deasserted (ap_rst_n = 1)\n");
    }

    /* 4. Khởi tạo cấu hình mặc định cho Pad 0 (Sink) và Pad 1 (Source) */
    holo->pads[0].flags = MEDIA_PAD_FL_SINK;
    holo->formats[0].width = 360;
    holo->formats[0].height = 360;
    holo->formats[0].code = MEDIA_BUS_FMT_RBG888_1X24;
    holo->formats[0].field = V4L2_FIELD_NONE;
    holo->formats[0].colorspace = V4L2_COLORSPACE_SRGB;

    holo->pads[1].flags = MEDIA_PAD_FL_SOURCE;
    holo->formats[1].width = 1920;
    holo->formats[1].height = 1080;
    holo->formats[1].code = MEDIA_BUS_FMT_RBG888_1X24;
    holo->formats[1].field = V4L2_FIELD_NONE;
    holo->formats[1].colorspace = V4L2_COLORSPACE_SRGB;

    /* 5. Đăng ký V4L2 Subdev */
    v4l2_subdev_init(&holo->subdev, &holo_ops);
    holo->subdev.owner = THIS_MODULE;
    holo->subdev.dev = &pdev->dev;
    v4l2_set_subdevdata(&holo->subdev, holo);
    snprintf(holo->subdev.name, sizeof(holo->subdev.name), "%s.%s",
             DRV_NAME, dev_name(&pdev->dev));
    holo->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    holo->subdev.entity.ops = &holo_media_ops;
    holo->subdev.entity.function = MEDIA_ENT_F_PROC_VIDEO_SCALER;

    ret = media_entity_pads_init(&holo->subdev.entity, 2, holo->pads);
    if (ret)
        return ret;

    platform_set_drvdata(pdev, holo);
    ret = v4l2_async_register_subdev(&holo->subdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register subdev\n");
        media_entity_cleanup(&holo->subdev.entity);
        return ret;
    }

    dev_info(&pdev->dev, "Xilinx Hologram Subdev registered successfully!\n");
    return 0;
}

static int holo_remove(struct platform_device *pdev) {
    struct holo_dev *holo = platform_get_drvdata(pdev);

    if (holo->rst_gpio)
        gpiod_set_value_cansleep(holo->rst_gpio, HOLO_RESET_ASSERT);

    v4l2_async_unregister_subdev(&holo->subdev);
    media_entity_cleanup(&holo->subdev.entity);
    if (holo->clk)
        clk_unprepare(holo->clk);
    return 0;
}

static const struct of_device_id holo_of_match[] = {
    { .compatible = "xlnx,hologram-splitter-4way-2.0", },
    { .compatible = "xlnx,hologram-splitter-1.0", },
    { }
};
MODULE_DEVICE_TABLE(of, holo_of_match);

static struct platform_driver holo_driver = {
    .probe = holo_probe,
    .remove = holo_remove,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = holo_of_match,
    },
};

module_platform_driver(holo_driver);

MODULE_AUTHOR("Cong Binh");
MODULE_DESCRIPTION("V4L2 Subdev Driver for Hologram Splitter IP");
MODULE_LICENSE("GPL");
