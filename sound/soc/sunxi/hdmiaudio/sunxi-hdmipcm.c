/*
 * sound\soc\sunxi\hdmiaudio\sunxi-hdmipcm.c
 * (C) Copyright 2010-2016
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * huangxin <huangxin@Reuuimllatech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <asm/dma.h>
#include <mach/hardware.h>
#include <sound/dmaengine_pcm.h>
#include <linux/dma/sunxi-dma.h>
#if defined CONFIG_ARCH_SUN9I || defined CONFIG_ARCH_SUN8IW6
#include "sunxi-hdmipcm.h"
#endif
#ifdef CONFIG_ARCH_SUN8IW7
#include "sunxi-hdmitdm.h"
#endif
#include <sound/asoundef.h>
int hdmi_format = 1;
atomic_t card_open_num;

static dma_addr_t hdmiraw_dma_addr = 0;
static dma_addr_t hdmipcm_dma_addr = 0;
static unsigned char *hdmiraw_dma_area;	/* DMA area */
static unsigned int numtotal = 0;

typedef struct headbpcuv {
	unsigned other:3;
	unsigned V:1;
	unsigned U:1;
	unsigned C:1;
	unsigned P:1;
	unsigned B:1;
} headbpcuv;

typedef union head61937
{
	headbpcuv head0;
	unsigned char head1;
} head61937;

// not sure if CRC is really needed, doesn't hurt
unsigned char crcTable[] = {
    0x00, 0x64, 0xC8, 0xAC, 0xE1, 0x85, 0x29, 0x4D, 0xB3, 0xD7, 0x7B, 0x1F, 0x52, 0x36, 0x9A, 0xFE,
    0x17, 0x73, 0xDF, 0xBB, 0xF6, 0x92, 0x3E, 0x5A, 0xA4, 0xC0, 0x6C, 0x08, 0x45, 0x21, 0x8D, 0xE9,
    0x2E, 0x4A, 0xE6, 0x82, 0xCF, 0xAB, 0x07, 0x63, 0x9D, 0xF9, 0x55, 0x31, 0x7C, 0x18, 0xB4, 0xD0,
    0x39, 0x5D, 0xF1, 0x95, 0xD8, 0xBC, 0x10, 0x74, 0x8A, 0xEE, 0x42, 0x26, 0x6B, 0x0F, 0xA3, 0xC7,
    0x5C, 0x38, 0x94, 0xF0, 0xBD, 0xD9, 0x75, 0x11, 0xEF, 0x8B, 0x27, 0x43, 0x0E, 0x6A, 0xC6, 0xA2,
    0x4B, 0x2F, 0x83, 0xE7, 0xAA, 0xCE, 0x62, 0x06, 0xF8, 0x9C, 0x30, 0x54, 0x19, 0x7D, 0xD1, 0xB5,
    0x72, 0x16, 0xBA, 0xDE, 0x93, 0xF7, 0x5B, 0x3F, 0xC1, 0xA5, 0x09, 0x6D, 0x20, 0x44, 0xE8, 0x8C,
    0x65, 0x01, 0xAD, 0xC9, 0x84, 0xE0, 0x4C, 0x28, 0xD6, 0xB2, 0x1E, 0x7A, 0x37, 0x53, 0xFF, 0x9B,
    0xB8, 0xDC, 0x70, 0x14, 0x59, 0x3D, 0x91, 0xF5, 0x0B, 0x6F, 0xC3, 0xA7, 0xEA, 0x8E, 0x22, 0x46,
    0xAF, 0xCB, 0x67, 0x03, 0x4E, 0x2A, 0x86, 0xE2, 0x1C, 0x78, 0xD4, 0xB0, 0xFD, 0x99, 0x35, 0x51,
    0x96, 0xF2, 0x5E, 0x3A, 0x77, 0x13, 0xBF, 0xDB, 0x25, 0x41, 0xED, 0x89, 0xC4, 0xA0, 0x0C, 0x68,
    0x81, 0xE5, 0x49, 0x2D, 0x60, 0x04, 0xA8, 0xCC, 0x32, 0x56, 0xFA, 0x9E, 0xD3, 0xB7, 0x1B, 0x7F,
    0xE4, 0x80, 0x2C, 0x48, 0x05, 0x61, 0xCD, 0xA9, 0x57, 0x33, 0x9F, 0xFB, 0xB6, 0xD2, 0x7E, 0x1A,
    0xF3, 0x97, 0x3B, 0x5F, 0x12, 0x76, 0xDA, 0xBE, 0x40, 0x24, 0x88, 0xEC, 0xA1, 0xC5, 0x69, 0x0D,
    0xCA, 0xAE, 0x02, 0x66, 0x2B, 0x4F, 0xE3, 0x87, 0x79, 0x1D, 0xB1, 0xD5, 0x98, 0xFC, 0x50, 0x34,
    0xDD, 0xB9, 0x15, 0x71, 0x3C, 0x58, 0xF4, 0x90, 0x6E, 0x0A, 0xA6, 0xC2, 0x8F, 0xEB, 0x47, 0x23,
};

static const struct snd_pcm_hardware sunxi_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
				      SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
				      SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	.rates			= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min		= 32000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 1024*1024,    /* value must be (2^n)Kbyte size */
	.period_bytes_min	= 156,
	.period_bytes_max	= 1024*1024,
	.periods_min		= 1,
	.periods_max		= 8,
	.fifo_size		= 128,
};

static unsigned int status = 0;
static unsigned char crc = 0;

static int  sunxi_hdmiaudio_ctl_iec958_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

static int sunxi_hdmiaudio_ctl_iec958_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int i;

	printk("> %s()\n", __func__);
	for (i = 0; i < 4; i++)
		ucontrol->value.iec958.status[i] =
			(status >> (i * 8)) & 0xff;

	return 0;
}

static int sunxi_hdmiaudio_ctl_iec958_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val = 0;
	int i, change;
	unsigned char newcrc = 0xff;
	unsigned char tmp;

	printk("> %s()\n", __func__);
	for (i = 0; i < 4; i++) {
		tmp = ucontrol->value.iec958.status[i];

		printk("> %s()[%d]: 0x%x\n", __func__, i, tmp);

		val |= (unsigned int)tmp << (i * 8);
		newcrc = crcTable[tmp ^ newcrc];
	}

	for(i = 0; i < 19; ++i)
		crc = crcTable[crc];

	change = val != status;
	status = val;
	crc = newcrc;

	if(status & IEC958_AES0_NONAUDIO) {
		if(((status >> 24) & IEC958_AES3_CON_FS) == IEC958_AES3_CON_FS_768000)
			hdmi_format = 11;
		else
			hdmi_format = 2;
	} else
		hdmi_format = 1;
	
	printk("[hdmi audio][sunxi-sndhdmi] Format: %d, CRC: %.2X\n", hdmi_format, crc);

	return change;
}

static const struct snd_kcontrol_new sunxi_hdmiaudio_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = sunxi_hdmiaudio_ctl_iec958_info,
		.get = sunxi_hdmiaudio_ctl_iec958_get,
		.put = sunxi_hdmiaudio_ctl_iec958_put,
	},
};

// borrowed from imx6 audio driver
static inline int parity(unsigned int a)
{
	//a ^= a >> 16;
	a ^= a >> 8;
	a ^= a >> 4;
	a ^= a >> 2;
	a ^= a >> 1;

	return a & 1;
}

int hdmi_transfer_format_61937_to_60958(unsigned char *out, unsigned char* in, size_t size)
{
	unsigned short *inSamples = (unsigned short*)in;
	unsigned int *outSamples = (unsigned int*)out;
	size_t count = size / 2;
	unsigned int sample;
	unsigned int curChBit;
	head61937 head;
	size_t i;

	
	head.head0.other = 0;
	head.head0.U = 0;
	head.head0.V = 1;

	for (i = 0; i < count; ++i) {
		curChBit = numtotal / 2;
		sample = *inSamples++;

		if (curChBit < 32)
			head.head0.C = (status >> curChBit) & 1;
		else if (curChBit < 184)
			head.head0.C = 0;
		else
			head.head0.C = (crc >> (curChBit - 184)) & 1;

		head.head0.P = parity(sample);
		head.head0.B = (curChBit > 0) ? 0 : 1;

		if (++numtotal == 384)
			numtotal = 0;

		sample <<= 11;
		sample |= ((unsigned int)(head.head1)) << 24;
		*outSamples++ = sample;
	}
	return 0;
}

static int sunxi_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct sunxi_dma_params *dmap;
	struct dma_slave_config slave_config;
	int ret;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	ret = snd_hwparams_to_dma_slave_config(substream, params,
						&slave_config);
	if (ret) {
		dev_err(dev, "hw params config failed with err %d\n", ret);
		return ret;
	}

	slave_config.dst_addr = dmap->dma_addr;
#ifdef CONFIG_ARCH_SUN8IW6
	slave_config.dst_maxburst = 8;
	slave_config.src_maxburst = 8;
#else
	slave_config.dst_maxburst = 4;
	slave_config.src_maxburst = 4;
#endif
#ifdef CONFIG_ARCH_SUN8IW1
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.slave_id = sunxi_slave_id(DRQDST_HDMI_AUDIO, DRQSRC_SDRAM);
#else
	if (SNDRV_PCM_FORMAT_S16_LE == params_format(params)) {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}

	printk("[hdmi audio][sunxi_pcm_hw_params] Format: %d\n", hdmi_format);
	if (hdmi_format > 1) {
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		strcpy(substream->pcm->card->id, "sndhdmiraw");
		hdmiraw_dma_area = dma_alloc_coherent(NULL, 2 * params_buffer_bytes(params),
							&hdmiraw_dma_addr, GFP_KERNEL);
		hdmipcm_dma_addr = substream->dma_buffer.addr;
		substream->dma_buffer.addr = hdmiraw_dma_addr;
	} else
		strcpy(substream->pcm->card->id, "sndhdmi");
	#ifdef CONFIG_ARCH_SUN9I
	slave_config.slave_id = sunxi_slave_id(DRQDST_DAUDIO_1_TX, DRQSRC_SDRAM);
	#endif
	#if defined (CONFIG_ARCH_SUN8IW7) || defined (CONFIG_ARCH_SUN8IW6)
	slave_config.slave_id = sunxi_slave_id(DRQDST_DAUDIO_2_TX, DRQSRC_SDRAM);
	#endif	
#endif

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret < 0) {
		dev_err(dev, "dma slave config failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	
	numtotal = 0;

	return 0;
}

static int sunxi_pcm_hw_free(struct snd_pcm_substream *substream)
{
	printk("[hdmi audio][sunxi_pcm_hw_free] Format: %d\n", hdmi_format);
	if (snd_pcm_lib_buffer_bytes(substream) && (hdmi_format > 1)) {
		dma_free_coherent(NULL, (2*snd_pcm_lib_buffer_bytes(substream)),
					      hdmiraw_dma_area, hdmiraw_dma_addr);
		substream->dma_buffer.addr = hdmipcm_dma_addr;
	}

	snd_pcm_set_runtime_buffer(substream, NULL);
  
	return 0;
}

static int sunxi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		snd_dmaengine_pcm_trigger(substream, SNDRV_PCM_TRIGGER_START);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		snd_dmaengine_pcm_trigger(substream, SNDRV_PCM_TRIGGER_STOP);
		break;
	}
	return 0;
}

static int sunxi_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	int ret = 0;

	ret = atomic_read(&card_open_num);
	if (ret > 0)
		return -EINVAL;

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &sunxi_pcm_hardware);

	ret = snd_dmaengine_pcm_open(substream, NULL, NULL);
	if (ret) {
		dev_err(dev, "dmaengine pcm open failed with err %d\n", ret);
		return ret;
	}
	atomic_inc(&card_open_num);

	return 0;
}

static int sunxi_pcm_close(struct snd_pcm_substream *substream)
{
	snd_dmaengine_pcm_close(substream);

	atomic_dec(&card_open_num);
	return 0;
}

static int sunxi_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = NULL;
	if (substream->runtime!=NULL) {
		runtime = substream->runtime;

		return dma_mmap_coherent(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
	}

	return -1;
}

static int sunxi_pcm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, hwoff);

	if (copy_from_user(hwbuf, buf, frames_to_bytes(runtime, frames)))
		return -EFAULT;

	printk("[hdmi audio][sunxi_pcm_copy] Format: %d\n", hdmi_format);
	if (hdmi_format > 1) {
		unsigned char* hdmihw_area = hdmiraw_dma_area + 2 * frames_to_bytes(runtime, hwoff);
		hdmi_transfer_format_61937_to_60958(hdmihw_area, hwbuf, frames_to_bytes(runtime, frames));
	}

	return ret;
}

static struct snd_pcm_ops sunxi_pcm_ops = {
	.open			= sunxi_pcm_open,
	.close			= sunxi_pcm_close,
	.ioctl			= snd_pcm_lib_ioctl,
	.hw_params		= sunxi_pcm_hw_params,
	.hw_free		= sunxi_pcm_hw_free,
	.trigger		= sunxi_pcm_trigger,

	.pointer		= snd_dmaengine_pcm_pointer_no_residue,
//	.pointer		= snd_dmaengine_pcm_pointer,

	.mmap			= sunxi_pcm_mmap,
	.copy			= sunxi_pcm_copy,
};

static void sunxi_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	int stream = SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;

	if (substream) {
		snd_dma_free_pages(&substream->dma_buffer);
		substream->dma_buffer.area = NULL;
		substream->dma_buffer.addr = 0;
	}
}

static u64 sunxi_pcm_mask = DMA_BIT_MASK(32);

static int sunxi_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm_substream *substream;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sunxi_pcm_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;

	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, pcm->card->dev,
			sunxi_pcm_hardware.buffer_bytes_max, &substream->dma_buffer);
	if (ret) {
		dev_err(card->dev, "failed to alloc playback dma buffer\n");
		return ret;
	}

	atomic_set(&card_open_num, 0);

	ret = snd_soc_add_codec_controls(rtd->codec, sunxi_hdmiaudio_controls,
					ARRAY_SIZE(sunxi_hdmiaudio_controls));
	if (ret)
		printk("[hdmi audio][sunxi-sndhdmi] Failed to register audio mode control, "
				"will continue without it.\n");
	return 0;
}

static struct snd_soc_platform_driver sunxi_soc_platform_hdmiaudio = {
	.ops = &sunxi_pcm_ops,
	.pcm_new = sunxi_pcm_new,
	.pcm_free = sunxi_pcm_free_dma_buffers,
};

static int sunxi_hdmiaudio_pcm_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &sunxi_soc_platform_hdmiaudio);
}

static int sunxi_hdmiaudio_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

/*data relating*/
static struct platform_device sunxi_hdmiaudio_pcm_device = {
	.name = "sunxi-hdmiaudio-pcm-audio",
};

static struct platform_driver sunxi_hdmiaudio_pcm_driver = {
	.probe = sunxi_hdmiaudio_pcm_probe,
	.remove = __exit_p(sunxi_hdmiaudio_pcm_remove),
	.driver = {
		.name 	= "sunxi-hdmiaudio-pcm-audio",
		.owner 	= THIS_MODULE,
	},
};

static int __init sunxi_soc_platform_hdmiaudio_init(void)
{
	int err;

	if ((err = platform_device_register(&sunxi_hdmiaudio_pcm_device)) < 0) {
		return err;
	}

	return platform_driver_register(&sunxi_hdmiaudio_pcm_driver);
}
module_init(sunxi_soc_platform_hdmiaudio_init);

static void __exit sunxi_soc_platform_hdmiaudio_exit(void)
{
	return platform_driver_unregister(&sunxi_hdmiaudio_pcm_driver);
}
module_exit(sunxi_soc_platform_hdmiaudio_exit);

MODULE_AUTHOR("huangxin");
MODULE_DESCRIPTION("SUNXI HDMIAUDIO DMA module");
MODULE_LICENSE("GPL");
