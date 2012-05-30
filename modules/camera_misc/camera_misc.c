/*
 * drivers/misc/camera_misc/camera_misc.c
 *
 * Copyright (C) 2010 Motorola Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define GPIO_SENSOR_NAME "camera_misc"
#define DEBUG 2

#define TAG "camera"

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/completion.h>
#include <linux/sched.h>

#include "camera_misc.h"
#include <../drivers/media/video/omap34xxcam.h>
#include <../drivers/media/video/isp/isp.h>
#include <../drivers/media/video/isp/ispreg.h>

#include <linux/gpio_mapping.h>
#include <linux/of.h>


#define err_print(fmt, args...) \
	printk(KERN_ERR "fun %s:"fmt, __func__, ##args)

#if DEBUG == 0
# define dbg_print(fmt, args...)  ;
# define ddbg_print(fmt, args...) ;
#elif DEBUG == 1
# define dbg_print(fmt, args...) printk(KERN_INFO "fun %s:"fmt, __func__, ##args)
# define ddbg_print(fmt, args...) ;
#else
# define dbg_print(fmt, args...) printk(KERN_INFO "fun %s:"fmt, __func__, ##args)
# define ddbg_print(fmt, args...) printk(KERN_INFO "fun %s:"fmt, __func__, ##args)
#endif

#include "../symsearch/symsearch.h"
#include <plat/dma.h>
#include <plat/vrfb.h>
#include <plat/display.h>
#include <../drivers/media/video/isp/ispccdc.h>
#include <../drivers/media/video/omap-vout/omapvout.h>
#include <../drivers/media/video/omap-vout/omapvout-dss.h>
#include "omapvout-mem.h"
#include "omapvout-vbq.h"
#include "omapvout-dss.h"
#include "hook.h"
#include "camise.h"
static bool hooked = false;

static struct proc_dir_entry *proc_root = NULL;

static void _camera_lines_lowpower_mode(void);
static void _camera_lines_func_mode(void);
static int camera_dev_open(struct inode *inode, struct file *file);
static int camera_dev_release(struct inode *inode, struct file *file);
static int camera_dev_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);
static int camera_misc_probe(struct platform_device *device);
static int camera_misc_remove(struct platform_device *device);
static int cam_misc_suspend(struct platform_device *pdev, pm_message_t state);
static int cam_misc_resume(struct platform_device *pdev);
static void cam_misc_disableClk(void);
static void cam_misc_enableClk(unsigned long clock);

static int gpio_reset;
static int gpio_powerdown;
static int isp_count_local=0;
static struct regulator *regulator;
static int bHaveResetGpio;
static int bHavePowerDownGpio;

static const struct file_operations camera_dev_fops = {
	.open       = camera_dev_open,
	.release    = camera_dev_release,
	.ioctl      = camera_dev_ioctl
};

static struct miscdevice cam_misc_device0 = {
	.minor      = MISC_DYNAMIC_MINOR,
	.name       = "camera0",
	.fops       = &camera_dev_fops
};

static struct platform_driver cam_misc_driver = {
	.probe = camera_misc_probe,
	.remove = camera_misc_remove,
	.suspend = cam_misc_suspend,
	.resume = cam_misc_resume,
	.driver = {
		.name = CAMERA_DEVICE0,
	},
};

static int camera_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int camera_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void cam_misc_disableClk()
{
	/* powerup the isp/cam domain on the 3410 */
	if (isp_count_local == 0) {
		isp_get();
		isp_count_local++;
	}
	isp_power_settings(1);
	isp_set_xclk(0, 0);

	/* Need to make sure that all encounters of the
	   isp clocks are disabled*/
	while (isp_count_local > 0) {
		isp_put();
		isp_count_local--;
	}
}

static void cam_misc_enableClk(unsigned long clock)
{
	/* powerup the isp/cam domain on the 3410 */
	if (isp_count_local == 0) {
		isp_get();
		isp_count_local++;
	}
	isp_power_settings(1);
	isp_set_xclk(clock, 0);
}

static int cam_misc_suspend(struct platform_device *pdev, pm_message_t state)
{
	dbg_print("------------- MOT CAM SUSPEND CALLED ---------");

	/* Checking to make sure that camera is on */
	if (bHavePowerDownGpio && (gpio_get_value(gpio_powerdown) == 0)) {
		/* If camera is on, need to pull standby pin high */
		gpio_set_value(gpio_powerdown, 1);
	}

	/* Per sensor specification, need a minimum of 20 uS of time when
	   standby pin pull high and when the master clock is turned off  */
	msleep(5);

	if (isp_count_local > 0) {
		/* Need to make sure that all encounters of the
		   isp clocks are disabled*/
		cam_misc_disableClk();
		dbg_print("CAMERA_MISC turned off MCLK done");
	}

	_camera_lines_lowpower_mode();

	/* Turn off power */
	if (regulator != NULL) {
		regulator_disable(regulator);
		regulator_put(regulator);
		regulator = NULL;
	}

	return 0;
}

static int cam_misc_resume(struct platform_device *pdev)
{
	return 0;
}

static int camera_dev_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	if (!bHaveResetGpio) {
		ddbg_print("Requesting reset gpio");
		rc = gpio_request(gpio_reset, "camera reset");
		if (!rc)
			bHaveResetGpio = 1;
	}
	if (!bHavePowerDownGpio) {
		ddbg_print("Requesting powerdown gpio");
		rc = gpio_request(gpio_powerdown, "camera powerdown");
		if (!rc)
			bHavePowerDownGpio = 1;
	}
	ddbg_print("ioctl cmd %u(arg=%lu)\n", cmd, arg);

	switch (cmd) {
	case CAMERA_RESET_WRITE: //100
		if (bHaveResetGpio) {
			gpio_direction_output(gpio_reset, 0);
			gpio_set_value(gpio_reset, (arg ? 1 : 0));
			dbg_print("CAMERA_MISC set RESET line to %u\n",
				(arg ? 1 : 0));
		}

		if (!bHaveResetGpio) {
			err_print ("CAMERA_MISC: gpio_request () failed. \
				rc = %d; cmd = %d; arg = %lu", rc, cmd, arg);
			return -EIO;
		}
		break;

	case CAMERA_POWERDOWN_WRITE: //101
		if (bHavePowerDownGpio) {
			gpio_direction_output(gpio_powerdown, 0);
			if (0 == arg)
				gpio_set_value(gpio_powerdown, 0);
			else
				gpio_set_value(gpio_powerdown, 1);
			dbg_print("CAMERA_MISC set POWERDOWN line to %u\n",
				(arg ? 1 : 0));
		}
		if (!bHavePowerDownGpio) {
			err_print ("CAMERA_MISC: gpio_request () failed. \
				rc = %d; cmd = %u; arg = %lu", rc, cmd, arg);
			return -EIO;
		}
		break;

	case CAMERA_CLOCK_DISABLE: //102
		cam_misc_disableClk();
		dbg_print("CAMERA_MISC turned off MCLK done");
		break;

	case CAMERA_CLOCK_ENABLE: //103
		cam_misc_enableClk(arg);
		dbg_print("CAMERA_MISC set MCLK to %d\n", (int) arg);
		break;
	case CAMERA_AVDD_POWER_ENABLE:  //104
		arg = 1;
	case CAMERA_AVDD_POWER_DISABLE: //105
		if (arg == 1) {
			/* turn on digital power */
			if (regulator != NULL) {
				err_print("Already have regulator");
			} else {
				regulator = regulator_get(NULL, "vcam");
				if (IS_ERR(regulator)) {
					err_print("Cannot get vcam regulator, \
						err=%ld\n", PTR_ERR(regulator));
					return PTR_ERR(regulator);
				} else {
					regulator_enable(regulator);
					msleep(5);
				}
			}
			_camera_lines_func_mode();
		} else {
			/* Turn off power */
			if (regulator != NULL) {
				regulator_disable(regulator);
				regulator_put(regulator);
				regulator = NULL;
			} else {
				err_print("Regulator for vcam is not initialized");
				return -EIO;
			}
			_camera_lines_lowpower_mode();
		}
		break;
	default:
		err_print("CAMERA_MISC received unsupported cmd; cmd = %u\n",
			cmd);
		return -EIO;
		break;
	}

	return 0;
}

#define CONTROL_PADCONF_CAM_FLD   0x48002114
#define CONTROL_PADCONF_CAM_XCLKA 0x48002110

static void _camera_lines_lowpower_mode(void)
{
	omap_writew(0x0007, 0x4800210C);   /* HSYNC */
	omap_writew(0x0007, 0x4800210E);   /* VSYNC */
	omap_writew(0x001F, 0x48002110);   /* MCLK */
	omap_writew(0x0007, 0x48002112);   /* PCLK */
	omap_writew(0x3A04, 0x48002114);   /* FLD */
	omap_writew(0x0007, 0x4800211A);   /* CAM_D2 */
	omap_writew(0x0007, 0x4800211C);   /* CAM_D3 */
	omap_writew(0x0007, 0x4800211E);   /* CAM_D4 */
	omap_writew(0x0007, 0x48002120);   /* CAM_D5 */
	omap_writew(0x0007, 0x48002122);   /* CAM_D6 */
	omap_writew(0x0007, 0x48002124);   /* CAM_D7 */
	omap_writew(0x001F, 0x48002126);   /* CAM_D8 */
	omap_writew(0x001F, 0x48002128);   /* CAM_D9 */
	omap_writew(0x001F, 0x4800212A);   /* CAM_D10 */
	omap_writew(0x001F, 0x4800212C);   /* CAM_D11 */
}

static void _camera_lines_func_mode(void)
{
	omap_writew(0x0118, 0x4800210C);  /* HSYNC */
	omap_writew(0x0118, 0x4800210E);  /* VSYNC */
	omap_writew(0x0000, 0x48002110);  /* MCLK */
	omap_writew(0x0118, 0x48002112);  /* PCLK */
	omap_writew(0x0004, 0x48002114);  /* FLD */
	omap_writew(0x0100, 0x4800211A);  /* CAM_D2 */
	omap_writew(0x0100, 0x4800211C);  /* CAM_D3 */
	omap_writew(0x0100, 0x4800211E);  /* CAM_D4 */
	omap_writew(0x0100, 0x48002120);  /* CAM_D5 */
	omap_writew(0x0100, 0x48002122);  /* CAM_D6 */
	omap_writew(0x0100, 0x48002124);  /* CAM_D7 */
	omap_writew(0x0100, 0x48002126);  /* CAM_D8 */
	omap_writew(0x0100, 0x48002128);  /* CAM_D9 */
	omap_writew(0x0100, 0x4800212A);  /* CAM_D10 */
	omap_writew(0x0100, 0x4800212C);  /* CAM_D11 */
}


// dump isp regs cache : cat /proc/camera/isp
static int proc_isp_read(char *page, char **start, off_t offset, int count, int *eof, void *data) {
	int wr=0;
/*	int r;
	unsigned int state=0;

	for (r=0; r<MAX_REGS; r++) if (store->r[r].reg) {

		state = (0xff & (unsigned int) store->r[r].last_value);
		wr += scnprintf(page + wr, count - wr, REG_FMT ":%s:%04x:%x\n", (unsigned int) r, capcap_regname(r), store->r[r].mask_wr, state);
	}
*/
	if (wr <= offset + count) *eof=1;

	*start = page + offset;
	wr -= offset;
	if (wr > count) wr = count;
	if (wr < 0) wr = 0;

	return wr;
}


/* PROBE / REMOVE */

static int __init camera_misc_probe(struct platform_device *device)
{
	printk(KERN_INFO "camera_misc_probe - probe function called");

	/* put the GPIO64 (camera powerdown) to default state
	Its getting altered by Jordan aptina sensor probe */
	omap_writew(0x001C, 0x480020D0);

	gpio_reset = get_gpio_by_name("gpio_cam_reset");
	gpio_powerdown = get_gpio_by_name("gpio_cam_pwdn");
	ddbg_print("gpio_cam_reset=%d gpio_cam_pwdn=%d", gpio_reset,
		gpio_powerdown);

	_camera_lines_lowpower_mode();

	/* Standby needs to be high */
	if (!gpio_request(gpio_powerdown, "camera powerdown")) {
		gpio_direction_output(gpio_powerdown, 0);
		gpio_set_value(gpio_powerdown, 1);

		/* Do not free gpio so that it cannot be overwritten */
		bHavePowerDownGpio = 1 ;
	}

	if (misc_register(&cam_misc_device0)) {
		err_print("error in register camera misc device!");
		return -EIO;
	}

	return 0;
}

static int camera_misc_remove(struct platform_device *device)
{
	misc_deregister(&cam_misc_device0);

	if (!bHaveResetGpio) {
		gpio_free(gpio_reset);
		bHaveResetGpio = 0;
	}
	if (!bHavePowerDownGpio) {
		gpio_free(gpio_powerdown);
		bHavePowerDownGpio = 0;
	}

	return 0;
}

/*
static int try_pix_parm(struct omap34xxcam_videodev *vdev,
                        struct v4l2_pix_format *best_pix_in,
                        struct v4l2_pix_format *wanted_pix_out,
                        struct v4l2_fract *best_ival)
{
	int fps;
	int fmtd_index;
	int rval;
	struct v4l2_pix_format best_pix_out;
	int in_aspect_ratio;
	int out_aspect_ratio;

	if (best_ival->numerator == 0
	    || best_ival->denominator == 0)
		*best_ival = vdev->vdev_sensor_config.ival_default;

	fps = best_ival->denominator / best_ival->numerator;

	best_ival->denominator = 0;
	best_pix_out.height = INT_MAX >> 1;
	best_pix_out.width = best_pix_out.height;


	for (fmtd_index = 0; fmtd_index < 64; fmtd_index++) {
		int size_index;
		struct v4l2_fmtdesc fmtd;

		fmtd.index = fmtd_index;
		fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		// this pixel format test is removed in defy+
		if (vdev->vfd->minor == CAM_DEVICE_SOC) {
			fmtd.pixelformat = V4L2_PIX_FMT_YUYV;
		}
		rval = vidioc_int_enum_fmt_cap(vdev->vdev_sensor, &fmtd);
		if (rval)
			break;
		ddbg_print("trying fmt %8.8x (%d.)\n", fmtd.pixelformat, fmtd_index);

		// Get supported resolutions.
		for (size_index = 0; size_index < 64; size_index++) {
			struct v4l2_frmsizeenum frms;
			struct v4l2_pix_format pix_tmp_in, pix_tmp_out;
			int ival_index;

			frms.index = size_index;
			frms.pixel_format = fmtd.pixelformat;
			rval = vidioc_int_enum_framesizes(vdev->vdev_sensor, &frms);
			if (rval)
				break;

			pix_tmp_in.pixelformat = frms.pixel_format;
			pix_tmp_in.width = frms.discrete.width;
			pix_tmp_in.height = frms.discrete.height;
			pix_tmp_out = *wanted_pix_out;
			rval = isp_try_fmt_cap(&pix_tmp_in, &pix_tmp_out);
			if (rval)
				return rval;

			ddbg_print("this w %d\th %d\tfmt %8.8x\t"
				"-> w %d\th %d\t fmt %8.8x"
				"\twanted w %d\th %d\t fmt %8.8x\n",
				pix_tmp_in.width, pix_tmp_in.height,
				pix_tmp_in.pixelformat,
				pix_tmp_out.width, pix_tmp_out.height,
				pix_tmp_out.pixelformat,
				wanted_pix_out->width, wanted_pix_out->height,
				wanted_pix_out->pixelformat);

			in_aspect_ratio = 0;
			out_aspect_ratio = 0;
			if (pix_tmp_out.width > pix_tmp_out.height) {
				in_aspect_ratio =
					(pix_tmp_in.width * 256)/
					pix_tmp_in.height;
				out_aspect_ratio =
					(pix_tmp_out.width * 256)/
					(pix_tmp_out.height);
			}

			if ((in_aspect_ratio -
					out_aspect_ratio) > 50) {
				ddbg_print("aspect ratio diff: "
					"w %d\th %d\tw %d\th %d\n",
					pix_tmp_out.width,
					pix_tmp_out.height,
					pix_tmp_in.width,
					pix_tmp_in.height);
				continue;
			}

#define SIZE_DIFF(pix1, pix2) \
		(abs((pix1)->width - (pix2)->width) \
		+ abs((pix1)->height - (pix2)->height))

			// Don't use modes that are farther from wanted size
			// that what we already got.
			if (SIZE_DIFF(&pix_tmp_out, wanted_pix_out)
			    > SIZE_DIFF(&best_pix_out, wanted_pix_out)) {
				ddbg_print("size diff bigger: "
					"w %d\th %d\tw %d\th %d\n",
					pix_tmp_out.width, pix_tmp_out.height,
					best_pix_out.width,
					best_pix_out.height);
				continue;
			}

			// There's an input mode that can provide output
			// closer to wanted.
			if (SIZE_DIFF(&pix_tmp_out, wanted_pix_out)
			    < SIZE_DIFF(&best_pix_out, wanted_pix_out)) {
				// Force renegotation of fps etc.
				best_ival->denominator = 0;
				ddbg_print("renegotiate: "
					"w %d\th %d\tw %d\th %d\n",
					pix_tmp_out.width, pix_tmp_out.height,
					best_pix_out.width,
					best_pix_out.height);
			}

			for (ival_index = 0; ; ival_index++) {
				struct v4l2_frmivalenum frmi;

				frmi.index = ival_index;
				frmi.pixel_format = frms.pixel_format;
				frmi.width = frms.discrete.width;
				frmi.height = frms.discrete.height;
				// FIXME: try to fix standard..
				frmi.reserved[0] = 0xdeafbeef;

				rval = vidioc_int_enum_frameintervals(
					vdev->vdev_sensor, &frmi);
				if (rval)
					break;

				ddbg_print("fps %d\n",
					frmi.discrete.denominator / frmi.discrete.numerator);

				if (best_ival->denominator == 0)
					goto do_it_now;

				// We aim to use maximum resolution
				// from the sensor, provided that the
				// fps is at least as close as on the
				// current mode.
#define FPS_ABS_DIFF(fps, ival) abs(fps - (ival).denominator / (ival).numerator)

				// Select mode with closest fps.
				if (FPS_ABS_DIFF(fps, frmi.discrete)
				    < FPS_ABS_DIFF(fps, *best_ival)) {
					ddbg_print("closer fps: "
						"fps %d\t fps %d\n",
						(int)FPS_ABS_DIFF(fps,
							     frmi.discrete),
						(int)FPS_ABS_DIFF(fps,
								  *best_ival));
					goto do_it_now;
				}

				// Select bigger resolution if it's available
				// at same fps.
				if (frmi.width + frmi.height
				    > best_pix_in->width + best_pix_in->height
				    && FPS_ABS_DIFF(fps, frmi.discrete)
				    <= FPS_ABS_DIFF(fps, *best_ival)) {
					ddbg_print("bigger res, "
						"same fps: "
						"w %d\th %d\tw %d\th %d\n",
						frmi.width, frmi.height,
						best_pix_in->width,
						best_pix_in->height);
					goto do_it_now;
				}

				dev_dbg(&vdev->vfd->dev, "falling through\n");

				continue;

do_it_now:
				*best_ival = frmi.discrete;
				best_pix_out = pix_tmp_out;
				best_pix_in->width = frmi.width;
				best_pix_in->height = frmi.height;
				best_pix_in->pixelformat = frmi.pixel_format;

				ddbg_print(
					"best_pix_in: w %d\th %d\tfmt %8.8x"
					"\tival %d/%d\n",
					best_pix_in->width,
					best_pix_in->height,
					best_pix_in->pixelformat,
					best_ival->numerator,
					best_ival->denominator);
			}
		}
	}

	if (best_ival->denominator == 0)
		return -EINVAL;

	*wanted_pix_out = best_pix_out;

	ddbg_print("w %d, h %d, fmt %8.8x -> w %d, h %d\n",
		 best_pix_in->width, best_pix_in->height,
		 best_pix_in->pixelformat,
		 best_pix_out.width, best_pix_out.height);

	return isp_try_fmt_cap(best_pix_in, wanted_pix_out);
	//HOOK_INVOKE(try_pix_parm, vdev, best_pix_in, wanted_pix_out, best_ival);
	//return ret;
}

void ispccdc_config_sync_if(struct ispccdc_syncif syncif)
{
	if (syncif.datsz == DAT8 && syncif.ipmod == YUV16 && syncif.vdpol == 1) {
		syncif.vdpol = 0;
	}
	HOOK_INVOKE(ispccdc_config_sync_if, syncif);
	return;
}
*/

SYMSEARCH_DECLARE_FUNCTION_STATIC(
	void, ss_omapvout_chk_overrides, struct omapvout_device *vout);

extern int cam_dss_acquire_vrfb(struct omapvout_device *vout);

int omapvout_open(struct file *file)
{
	struct omapvout_device *vout;
	u16 w, h;
	int rc;

	vout = video_drvdata(file);

	if (vout == NULL) {
		printk(KERN_ERR "Invalid device\n");
		return -ENODEV;
	}

	mutex_lock(&vout->mtx);

	/* We only support single open */
	if (vout->opened) {
		ddbg_print(": Device already opened\n");
		cam_dss_release(vout);
		rc = -EBUSY;
		goto failed;
	}

	//cam_dss_remove(vout);
	//cam_dss_acquire_vrfb(vout);
	//cam_dss_init(vout,);
	rc = cam_dss_open(vout, &w, &h);
	if (rc != 0)
		goto failed;

	dbg_print("Overlay Display w/h %dx%d\n", w, h);

	if (w == 0 || h == 0) {
		printk(KERN_ERR "Invalid display resolution\n");
		rc = -EINVAL;
		goto failed;
	}

	rc = cam_vbq_init(vout);
	if (rc != 0)
		goto failed;

	vout->disp_width = w;
	vout->disp_height = h;
	vout->opened = 1;

	memset(&vout->pix, 0, sizeof(vout->pix));
	vout->pix.width = w;
	vout->pix.height = h;
	vout->pix.field = V4L2_FIELD_NONE;
	vout->pix.pixelformat = V4L2_PIX_FMT_YUYV; /* Arbitrary */
	vout->pix.colorspace = V4L2_COLORSPACE_SRGB; /* Arbitrary */
	vout->pix.bytesperline = w * 2;
	vout->pix.sizeimage = w * h * 2;

	memset(&vout->win, 0, sizeof(vout->win));
	vout->win.w.width = w;
	vout->win.w.height = h;
	vout->win.field = V4L2_FIELD_NONE;

	memset(&vout->crop, 0, sizeof(vout->crop));
	vout->crop.width = w;
	vout->crop.height = h;

	memset(&vout->fbuf, 0, sizeof(vout->fbuf));

	vout->rotation = 0;
	vout->bg_color = 0;

	vout->mmap_cnt = 0;

	SYMSEARCH_BIND_FUNCTION_TO(camera, omapvout_chk_overrides, ss_omapvout_chk_overrides);
	ss_omapvout_chk_overrides(vout);

	mutex_unlock(&vout->mtx);

	file->private_data = vout;

	ddbg_print("done");
	return 0;

failed:
	printk(KERN_ERR "fail !\n");
	mutex_unlock(&vout->mtx);
	return rc;
}

struct hook_info g_hi[] = {
//	HOOK_INIT(try_pix_parm),
//	HOOK_INIT(ispccdc_config_sync_if),
	HOOK_INIT(omapvout_open),
	HOOK_INIT_END
};

/* UNREGISTER/FREE HP/3A (Red Lens Camera)
SYMSEARCH_DECLARE_FUNCTION_STATIC(void, ss_deinitialize_hp3a_framework, void);

static int cam_hp3a_remove(struct platform_device *device)
{
	SYMSEARCH_BIND_FUNCTION_TO(camera, deinitialize_hp3a_framework, ss_deinitialize_hp3a_framework);
	// Deinitialize 3A task, this is a blocking call.
	ss_deinitialize_hp3a_framework();
	return 0;
}
static struct platform_driver cam_hp3a_driver = {
	.remove = cam_hp3a_remove,
	.driver = {
			.name = "hp3a",
	},
};
*/

/* in board_defs.c */
extern int  camise_add_i2c_device(void);
extern void camise_del_i2c_device(void);
extern void hplens_i2c_release(void);

static int __init camera_misc_init(void)
{
	int ret=0;
	struct proc_dir_entry *proc_entry;

	dbg_print("unregister red lens\n");
	hplens_i2c_release();

	//dbg_print("unregister hp3a_driver\n");
	//platform_driver_unregister(&cam_hp3a_driver);

	camise_add_i2c_device();
	ret=platform_driver_register(&cam_misc_driver);

	proc_root = proc_mkdir(TAG, NULL);
	proc_entry = create_proc_read_entry("isp", 0444, proc_root, proc_isp_read, NULL);
	//proc_entry->write_proc = ;

	hook_init();
	hooked = true;

	return ret;
}

static void __exit camera_misc_exit(void)
{
	remove_proc_entry("isp", proc_root);
	remove_proc_entry(TAG, NULL);
	if (hooked) hook_exit();
	platform_driver_unregister(&cam_misc_driver);
	camise_del_i2c_device();
}

module_init(camera_misc_init);
module_exit(camera_misc_exit);

MODULE_VERSION("1.3");
MODULE_DESCRIPTION("Motorola Camera Driver Backport");
MODULE_AUTHOR("Tanguy Pruvot, From Motorola Open Sources");
MODULE_LICENSE("GPL");
