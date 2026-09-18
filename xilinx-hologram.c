#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define DRV_NAME "xilinx-hologram"

/* AXI4-Lite Control Registers cho HLS IP */
#define HOLO_CTRL_REG       0x00
#define HOLO_START_AUTO     0x81
#define HOLO_STOP           0x00

struct holo_dev {
    struct v4l2_subdev subdev;
    struct media_pad pads[2];
    void __iomem *base;
    struct clk *clk;
    struct gpio_desc *rst_gpio;
    struct v4l2_mbus_framefmt formats[2];
};

static inline struct holo_dev *to_holo_dev(struct v4l2_subdev *sd) {
    return container_of(sd, struct holo_dev, subdev);
}

/* Điều khiển luồng: Bật/Tắt IP khi app gọi STREAMON/STREAMOFF */
static int holo_s_stream(struct v4l2_subdev *sd, int enable) {
    struct holo_dev *holo = to_holo_dev(sd);

    if (enable) {
        clk_enable(holo->clk);
        iowrite32(HOLO_START_AUTO, holo->base + HOLO_CTRL_REG);
        dev_info(sd->dev, "Hologram IP: Stream Started (CTRL: 0x%02x)\n",
                 ioread32(holo->base + HOLO_CTRL_REG));
    } else {
        iowrite32(HOLO_STOP, holo->base + HOLO_CTRL_REG);
        clk_disable(holo->clk);
        dev_info(sd->dev, "Hologram IP: Stream Stopped\n");
    }
    return 0;
}

/* Trả về format của Pad */
static int holo_get_fmt(struct v4l2_subdev *sd,
                        struct v4l2_subdev_state *sd_state,
                        struct v4l2_subdev_format *fmt) {
    struct holo_dev *holo = to_holo_dev(sd);
    if (fmt->pad >= 2)
        return -EINVAL;

    fmt->format = holo->formats[fmt->pad];
    return 0;
}

/* Thiết lập format cho Pad */
static int holo_set_fmt(struct v4l2_subdev *sd,
                        struct v4l2_subdev_state *sd_state,
                        struct v4l2_subdev_format *fmt) {
    struct holo_dev *holo = to_holo_dev(sd);
    if (fmt->pad >= 2)
        return -EINVAL;

    holo->formats[fmt->pad] = fmt->format;
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
    clk_prepare(holo->clk);

    /* 3. Quản lý Reset GPIO: Nhả reset phần cứng (ap_rst_n = 1) */
    holo->rst_gpio = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(holo->rst_gpio)) {
        dev_err(&pdev->dev, "Failed to get reset GPIO\n");
        return PTR_ERR(holo->rst_gpio);
    }

    if (holo->rst_gpio) {
        /* GPIOD_OUT_LOW kết hợp cờ Active-Low trong Device Tree (<&gpio 83 1>)
           sẽ kéo chân vật lý ap_rst_n lên mức 1 */
        gpiod_set_value_cansleep(holo->rst_gpio, 0);
        usleep_range(2000, 5000);
        dev_info(&pdev->dev, "Hologram IP: Hardware reset released (ap_rst_n = 1)\n");
    }

    /* 4. Khởi tạo Pads: Pad 0 = SINK (360x360), Pad 1 = SOURCE (1920x1080) */
    holo->pads[0].flags = MEDIA_PAD_FL_SINK;
    holo->pads[1].flags = MEDIA_PAD_FL_SOURCE;

    holo->formats[0].width = 360;
    holo->formats[0].height = 360;
    holo->formats[0].code = MEDIA_BUS_FMT_RBG888_1X24;
    holo->formats[0].field = V4L2_FIELD_NONE;

    holo->formats[1].width = 1920;
    holo->formats[1].height = 1080;
    holo->formats[1].code = MEDIA_BUS_FMT_RBG888_1X24;
    holo->formats[1].field = V4L2_FIELD_NONE;

    /* 5. Đăng ký V4L2 Subdev */
    v4l2_subdev_init(&holo->subdev, &holo_ops);
    holo->subdev.owner = THIS_MODULE;
    holo->subdev.dev = &pdev->dev;
    v4l2_set_subdevdata(&holo->subdev, holo);
    snprintf(holo->subdev.name, sizeof(holo->subdev.name), "%s.%s",
             DRV_NAME, dev_name(&pdev->dev));
    holo->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
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

    /* Đưa IP về trạng thái Reset khi gỡ driver */
    if (holo->rst_gpio)
        gpiod_set_value_cansleep(holo->rst_gpio, 1);

    v4l2_async_unregister_subdev(&holo->subdev);
    media_entity_cleanup(&holo->subdev.entity);
    clk_unprepare(holo->clk);
    return 0;
}

static const struct of_device_id holo_of_match[] = {
    { .compatible = "xlnx,hologram-splitter-4way-2.0", },
    { .compatible = "xlnx,hologram-splitter-1.0", },
    { /* sentinel */ }
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
