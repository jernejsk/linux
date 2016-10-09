#include "hdmi_core.h"

static __s32 is_hdmi;
static __s32 is_yuv;
__s32 is_exp = 0;
__u32 rgb_only = 0;
__u8 EDID_Buf[HDMI_EDID_LEN];
__u8 Device_Support_VIC[HDMI_DEVICE_SUPPORT_VIC_SIZE];
extern void hdmi_edid_received(unsigned char *edid, int block);

static __u8 exp0[16] =
{
	0x36,0x74,0x4d,0x53,0x74,0x61,0x72,0x20,0x44,0x65,0x6d,0x6f,0x0a,0x20,0x20,0x38
};

static __u8 exp1[16] =
{
	0x2d,0xee,0x4b,0x4f,0x4e,0x41,0x4b,0x20,0x54,0x56,0x0a,0x20,0x20,0x20,0x20,0xa5
};

static int GetEDIDData(__u8 block, __u8 *buf)
{
	__u8 *pbuf = buf + 128 * block;
	__u8 offset = (block & 0x01) ? 128 : 0;

	return bsp_hdmi_ddc_read(Explicit_Offset_Address_E_DDC_Read,block >> 1, offset, 128, pbuf);
}

static __s32
EDID_CheckSum(__u8 block, __u8 *buf)
{
	__s32 i = 0, CheckSum = 0;
	__u8 *pbuf = buf + 128 * block;

	for( i = 0, CheckSum = 0 ; i < 128 ; i++ ) {
		CheckSum += pbuf[i];
		CheckSum &= 0xFF;
	}

	return CheckSum;
}

static __s32
EDID_Header_Check(__u8 *pbuf)
{
	if (pbuf[0] != 0x00 || pbuf[1] != 0xFF || pbuf[2] != 0xFF ||
	    pbuf[3] != 0xFF || pbuf[4] != 0xFF || pbuf[5] != 0xFF ||
	    pbuf[6] != 0xFF || pbuf[7] != 0x00) {
		pr_info("EDID block0 header error\n");
		return -1;
	}

	return 0;
}

static __s32
EDID_Version_Check(__u8 *pbuf)
{
	pr_info("EDID version: %d.%d ",pbuf[0x12],pbuf[0x13]) ;
	if (pbuf[0x12] != 0x01) {
		pr_info("Unsupport EDID format, EDID parsing exit\n");
		return -1;
	}
	if (pbuf[0x13] < 3 && !(pbuf[0x18] & 0x02)) {
		pr_info("EDID revision < 3 and preferred timing feature bit "
			"not set, ignoring EDID info\n");
		return -1;
	}

	return 0;
}

static __s32
Parse_DTD_Block(__u8 *pbuf)
{
	__u32 pclk, sizex, Hblanking, sizey, Vblanking, Hsync_offset,
		Hsync_pulsew, Vsync_offset, Vsync_pulsew, H_image_size,
		V_image_size, H_Border, V_Border, pixels_total, frame_rate,
		Hsync, Vsync, HT, VT;

	pclk = (((__u32) pbuf[1] << 8) + pbuf[0]) * 10000;
	sizex = (((__u32) pbuf[4] << 4) & 0x0f00) + pbuf[2];
	Hblanking = (((__u32) pbuf[4] << 8) & 0x0f00) + pbuf[3];
	sizey = (((__u32) pbuf[7] << 4) & 0x0f00) + pbuf[5];
	Vblanking = (((__u32) pbuf[7] << 8) & 0x0f00) + pbuf[6];
	Hsync_offset = (((__u32) pbuf[11] << 2) & 0x0300) + pbuf[8];
	Hsync_pulsew = (((__u32) pbuf[11] << 4) & 0x0300) + pbuf[9];
	Vsync_offset = (((__u32) pbuf[11] << 2) & 0x0030) + (pbuf[10] >> 4);
	Vsync_pulsew = (((__u32) pbuf[11] << 4) & 0x0030) + (pbuf[10] & 0x0f);
	H_image_size = (((__u32) pbuf[14] << 4) & 0x0f00) + pbuf[12];
	V_image_size = (((__u32) pbuf[14] << 8) & 0x0f00) + pbuf[13];
	H_Border = pbuf[15];
	V_Border = pbuf[16];
	Hsync = (pbuf[17] & 0x02) >> 1;
	Vsync = (pbuf[17] & 0x04) >> 2;
	HT = sizex + Hblanking;
	VT = sizey + Vblanking;

	pixels_total = HT * VT;

	if ((pbuf[0] == 0) && (pbuf[1] == 0) && (pbuf[2] == 0))
		return 0;

	if (pixels_total == 0)
		return 0;
	else
		frame_rate = pclk / pixels_total;

	if ((frame_rate == 59) || (frame_rate == 60)) {
		if ((sizex == 720) && (sizey == 240))
			Device_Support_VIC[HDMI1440_480I] = 1;

		if ((sizex == 720) && (sizey == 480))
			Device_Support_VIC[HDMI480P] = 1;

		if ((sizex == 1280) && (sizey == 720))
			Device_Support_VIC[HDMI720P_60] = 1;

		if ((sizex == 1920) && (sizey == 540))
			Device_Support_VIC[HDMI1080I_60] = 1;

		if ((sizex == 1920) && (sizey == 1080))
			Device_Support_VIC[HDMI1080P_60] = 1;

	} else if ((frame_rate == 49) || (frame_rate == 50)) {
		if ((sizex == 720) && (sizey == 288))
			Device_Support_VIC[HDMI1440_576I] = 1;

		if ((sizex == 720) && (sizey == 576))
			Device_Support_VIC[HDMI576P] = 1;

		if ((sizex == 1280) && (sizey == 720))
			Device_Support_VIC[HDMI720P_50] = 1;

		if ((sizex == 1920) && (sizey == 540))
			Device_Support_VIC[HDMI1080I_50] = 1;

		if ((sizex == 1920) && (sizey == 1080))
			Device_Support_VIC[HDMI1080P_50] = 1;

	} else if ((frame_rate == 23) || (frame_rate == 24)) {
		if ((sizex == 1920) && (sizey == 1080))
			Device_Support_VIC[HDMI1080P_24] = 1;
	}

	pr_info("PCLK=%d X %d %d %d %d Y %d %d %d %d fr %d %s%s\n", pclk,
		sizex, sizex + Hsync_offset,
		sizex + Hsync_offset + Hsync_pulsew, HT,
		sizey, sizey + Vsync_offset,
		sizey + Vsync_offset + Vsync_pulsew, VT,
		frame_rate, Hsync ? "P" : "N", Vsync ? "P" : "N");

	/* Pick the first mode with a width which is a multiple of 8 and
	   a supported pixel-clock */
	if (Device_Support_VIC[HDMI_EDID] || (sizex & 7))
		return 0;

	video_timing[video_timing_edid].tv_mode = 0;
	video_timing[video_timing_edid].pixel_clk = pclk;
	video_timing[video_timing_edid].pixel_repeat = 0;
	video_timing[video_timing_edid].x_res = sizex;
	video_timing[video_timing_edid].y_res = sizey;
	video_timing[video_timing_edid].hor_total_time = HT;
	video_timing[video_timing_edid].hor_back_porch = Hblanking - Hsync_offset;
	video_timing[video_timing_edid].hor_front_porch = Hsync_offset;
	video_timing[video_timing_edid].hor_sync_time = Hsync_pulsew;
	video_timing[video_timing_edid].ver_total_time = VT;
	video_timing[video_timing_edid].ver_back_porch = Vblanking - Vsync_offset;
	video_timing[video_timing_edid].ver_front_porch = Vsync_offset;
	video_timing[video_timing_edid].ver_sync_time = Vsync_pulsew;
	video_timing[video_timing_edid].hor_sync_polarity = Hsync;
	video_timing[video_timing_edid].ver_sync_polarity = Vsync;
	video_timing[video_timing_edid].b_interlace = (pbuf[17] & 0x80) >> 7;
	video_timing[video_timing_edid].vactive_space = 0;
	video_timing[video_timing_edid].trd_mode = 0;
	
	pr_info("Using above mode as preferred EDID mode\n");

	if (video_timing[video_timing_edid].b_interlace) {
		video_timing[video_timing_edid].y_res *= 2;
		video_timing[video_timing_edid].ver_total_time *= 2;

		/* Should VT be VT * 2 + 1, or VT * 2 ? */
		frame_rate = (frame_rate + 1) / 2;
		if ((HT * (VT * 2 + 1) * frame_rate) == pclk)
			video_timing[video_timing_edid].ver_total_time++;

		pr_info("Interlaced VT %d\n",
			video_timing[video_timing_edid].ver_total_time);
	}
	Device_Support_VIC[HDMI_EDID] = 1;

	return 0;
}

static __s32
Parse_VideoData_Block(__u8 *pbuf, __u8 size)
{
	int i;

	for (i = 0; i < size; i++) {
		Device_Support_VIC[pbuf[i] & 0x7f] = 1;
		pr_info("Parse_VideoData_Block: VIC %d%s support\n",
			pbuf[i] & 0x7f, (pbuf[i] & 0x80) ? " (native)" : "");
	}

	return 0;
}

static __s32
Parse_AudioData_Block(__u8 *pbuf, __u8 size)
{
	__u8 sum;

	for (sum = 0; sum < size; sum += 3)
		if ((pbuf[sum] & 0xf8) == 0x08) {
			pr_info("Parse_AudioData_Block: max channel=%d\n",
				(pbuf[sum]&0x7)+1);
			pr_info("Parse_AudioData_Block: SampleRate code=%x\n",
				pbuf[sum+1]);
			pr_info("Parse_AudioData_Block: WordLen code=%x\n",
				pbuf[sum+2]);
		}

	return 0;
}

static __s32
Parse_HDMI_VSDB(__u8 * pbuf, __u8 size)
{
	__u8 index = 8;
	__u8 vic_len = 0;
	__u8 i;

	/* check if it's HDMI VSDB */
	if ((pbuf[0] ==0x03) && (pbuf[1] ==0x0c) && (pbuf[2] ==0x00)) {
		is_hdmi = 1;
		pr_info("Find HDMI Vendor Specific DataBlock\n");
	} else
		return 0;

	if (size <= 8)
		return 0;

	if ((pbuf[7] & 0x20) == 0 )
		return 0;
	if ((pbuf[7] & 0x40) == 0x40 )
		index = index + 2;
	if ((pbuf[7] & 0x80) == 0x80 )
		index = index + 2;

	/* mandatory format support */
	if (pbuf[index] & 0x80) {
		Device_Support_VIC[HDMI1080P_24_3D_FP] = 1;
		Device_Support_VIC[HDMI720P_50_3D_FP] = 1;
		Device_Support_VIC[HDMI720P_60_3D_FP] = 1;
		pr_info("3D_present\n");
	} else
		return 0;

	if ((pbuf[index] & 0x60) != 0)
		pr_info("3D_multi_present\n");

	vic_len = pbuf[index+1] >> 5;
	for (i = 0; i < vic_len; i++) {
		/* HDMI_VIC for extended resolution transmission */
		Device_Support_VIC[pbuf[index+1+1+i] + 0x100] = 1;
		pr_info("Parse_HDMI_VSDB: VIC %d support\n", pbuf[index+1+1+i]);
	}

	index += (pbuf[index+1] & 0xe0) + 2;
	if(index > (size+1) )
	    return 0;

	pr_info("3D_multi_present byte(%2.2x,%2.2x)\n", pbuf[index],
		pbuf[index+1]);

	return 0;
}

static __s32 ParseEDID_CEA861_extension_block(__u32 i, __u8 *EDID_Buf)
{
	__u32 offset;
	if (EDID_Buf[0x80 * i + 3] & 0x20) {
		is_yuv = 1;
		pr_info("device support YCbCr44 output\n");
		if(rgb_only == 1) {
			pr_info("rgb only test!\n");
			is_yuv = 0;
		}
	}

	offset = EDID_Buf[0x80 * i + 2];
	/* deal with reserved data block */
	if (offset > 4)	{
		__u8 bsum = 4;
		while (bsum < offset) {
			__u8 tag = EDID_Buf[0x80 * i + bsum] >> 5;
			__u8 len = EDID_Buf[0x80 * i + bsum] & 0x1f;
			if ((len > 0) && ((bsum + len + 1) > offset)) {
				pr_info("len or bsum size error\n");
				return 0;
			} else {
				if (tag == 1) { /* ADB */
					Parse_AudioData_Block(EDID_Buf + 0x80 * i + bsum + 1, len);
				} else if (tag == 2) { /* VDB */
					Parse_VideoData_Block(EDID_Buf + 0x80 * i + bsum + 1, len);
				} else if (tag == 3) { /* vendor specific */
					Parse_HDMI_VSDB(EDID_Buf + 0x80 * i + bsum + 1, len);
				}
			}

			bsum += (len + 1);
		}
	} else
		pr_info("no data in block%d\n", i);

	if (offset >= 4) { /* deal with 18-byte timing block */
		while (offset < (0x80 - 18)) {
			Parse_DTD_Block(EDID_Buf + 0x80 * i + offset);
			offset += 18;
		}
	} else
		pr_info("no DTD in block%d\n", i);

	return 1;
}

#define TRIES 3
static int get_edid_block(int block, unsigned char *buf)
{
	int i;

	for (i = 1; i <= TRIES; i++) {
		if (GetEDIDData(block, buf)) {
			pr_warn("unable to read EDID block %d, try %d/%d\n",
				block, i, TRIES);
			continue;
		}
		if (EDID_CheckSum(block, buf) != 0) {
			pr_warn("EDID block %d checksum error, try %d/%d\n",
				block, i, TRIES);
			continue;
		}
		break;
	}
	return (i <= TRIES) ? 0 : -EIO;
}

static __s32 Check_EDID(__u8 *buf_src, __u8*buf_dst)
{
	__u32 i;
	
	for (i = 0; i < 2; i++)
		if (buf_dst[i] != buf_src[8 + i])
			return -1;

	for (i = 0; i < 13; i++)
		if (buf_dst[2 + i] != buf_src[0x5f + i])
			return -1;

	if(buf_dst[15] != buf_src[0x7f])
		return -1;
	
	return 0;
}


__s32 ParseEDID(void)
{
	__u8 BlockCount;
	__u32 i;

	pr_info("ParseEDID\n");

	if (get_video_mode() == HDMI_EDID) {
		/* HDMI_DEVICE_SUPPORT_VIC_SIZE - 1 so as to not overwrite
		   the currently in use timings with a new preferred mode! */
		memset(Device_Support_VIC, 0,
		       HDMI_DEVICE_SUPPORT_VIC_SIZE - 1);
	} else {
		memset(Device_Support_VIC, 0, HDMI_DEVICE_SUPPORT_VIC_SIZE);
	}

	memset(EDID_Buf,0,sizeof(EDID_Buf));

	is_hdmi = 0;
	is_yuv = 0;
	is_exp = 0;

	if (get_edid_block(0, EDID_Buf))
		return 0;

	if (EDID_Header_Check(EDID_Buf) !=  0)
		return 0;

	if (EDID_Version_Check(EDID_Buf) != 0)
		return 0;

	Parse_DTD_Block(EDID_Buf + 0x36);

	Parse_DTD_Block(EDID_Buf + 0x48);

	/* Check for "MStar Demo" or "Konak TV" display, which
	   supposedly misinform about supported color space */
	if ((Check_EDID(EDID_Buf,exp0) == 0) ||
	    (Check_EDID(EDID_Buf,exp1) == 0))
	{
		printk("*****************is_exp*****************\n");
		is_exp = 1;
	}

	BlockCount = EDID_Buf[0x7E] + 1;
	if (BlockCount > 5)
		BlockCount = 5;

	for (i = 1; i < BlockCount; i++) {
		if (get_edid_block(i, EDID_Buf) != 0) {
			BlockCount = i;
			break;
		}
	}

	for (i = 1; i < BlockCount; i++) {
		if (EDID_Buf[0x80 * i + 0] == 2) {
			if (!ParseEDID_CEA861_extension_block(i, EDID_Buf))
				return 0;
		}
	}

	return 0;
}

__u32 GetIsHdmi(void)
{
	return is_hdmi;
}

__u32 GetIsYUV(void)
{
	return is_yuv;
}

__s32 GetEdidInfo(void)
{
	return (__s32)EDID_Buf;
}

