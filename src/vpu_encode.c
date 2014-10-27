/*
 * Copyright (c) 2014 Andy Huang <andyspider@126.com>
 *
 * This file is part of Camkit.
 *
 * Camkit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Camkit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Camkit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <vpu_lib.h>
#include <vpu_io.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "camkit/encode.h"

#define STREAM_BUF_SIZE 0x200000
#define NUM_FRAME_BUFS 32
#define FB_INDEX_MASK (NUM_FRAME_BUFS - 1)

struct my_frame_buf
{
	int addrY;
	int addrCb;
	int addrCr;
	int strideY;
	int strideC;
	int mvColBuf;
	vpu_mem_desc desc;
};

struct enc_handle
{
	int init_vpu;
	vpu_mem_desc mem_desc;
	EncHandle vpu_enchandle;
	PhysicalAddress phy_bsbuf_addr;
	U32 virt_bsbuf_addr;
	int min_fb_count;
	int totalfb;
	int src_fbid;
	struct my_frame_buf *myfbarray[NUM_FRAME_BUFS];
	struct my_frame_buf myfbpool[NUM_FRAME_BUFS];
	struct my_frame_buf **pfbpool;
	int myfb_index;
	FrameBuffer *fb;
	int quit;
	unsigned long frame_counter;
	int gop_offset_counter;
	int yuv420_imgsize;
	U32 y_addr;
	int mvc_extension;
	EncParam enc_param;

	struct enc_param params;
};

static void init_framebuf(struct enc_handle *handle)
{
	int i;
	for (i = 0; i < NUM_FRAME_BUFS; i++)
	{
		handle->myfbarray[i] = &handle->myfbpool[i];
	}

	handle->myfb_index = 0;
}

static struct my_frame_buf *get_framebuf(struct enc_handle *handle)
{
	struct my_frame_buf *myfb;
	myfb = handle->myfbarray[handle->myfb_index];
	handle->myfbarray[handle->myfb_index] = NULL;

	++handle->myfb_index;
	handle->myfb_index &= FB_INDEX_MASK;

	return myfb;
}

static void put_framebuf(struct enc_handle *handle, struct my_frame_buf *myfb)
{
	--handle->myfb_index;
	handle->myfb_index &= FB_INDEX_MASK;

	handle->myfbarray[handle->myfb_index] = myfb;
}

static struct my_frame_buf *framebuf_alloc(struct enc_handle *handle,
		int strideY, int height, int mvCol)
{
	struct my_frame_buf *mfb;
	int ret;
	int divX, divY;

	mfb = get_framebuf(handle);
	if (mfb == NULL)
	{
		printf("--- get_framebuf failed\n");
		return NULL;
	}

	divX = divY = 2;

	memset(&(mfb->desc), 0, sizeof(vpu_mem_desc));
	mfb->desc.size = (strideY * height + strideY / divX * height / divY * 2);
	if (mvCol)
		mfb->desc.size += strideY / divX * height / divY;

	ret = IOGetPhyMem(&(mfb->desc));
	if (ret)
	{
		printf("--- alloc frame buffer failed\n");
		memset(&(mfb->desc), 0, sizeof(vpu_mem_desc));
		return NULL;
	}

	mfb->addrY = mfb->desc.phy_addr;
	mfb->addrCb = mfb->addrY + strideY * height;
	mfb->addrCr = mfb->addrCb + strideY / divX * height / divY;
	mfb->strideY = strideY;
	mfb->strideC = strideY / divX;
	if (mvCol)
		mfb->mvColBuf = mfb->addrCr + strideY / divX * height / divY;

	mfb->desc.virt_uaddr = IOGetVirtMem(&(mfb->desc));
	if (mfb->desc.virt_uaddr <= 0)
	{
		IOFreePhyMem(&mfb->desc);
		memset(&(mfb->desc), 0, sizeof(vpu_mem_desc));
		return NULL;
	}

	return mfb;
}

static void framebuf_free(struct enc_handle *handle, struct my_frame_buf *mfb)
{
	if (mfb == NULL)
		return;

	if (mfb->desc.virt_uaddr)
	{
		IOFreeVirtMem(&mfb->desc);
	}

	if (mfb->desc.phy_addr)
	{
		IOFreePhyMem(&mfb->desc);
	}

	memset(&(mfb->desc), 0, sizeof(vpu_mem_desc));
	put_framebuf(handle, mfb);
}

static int encoder_allocate_framebuffer(struct enc_handle *handle)
{
	int extrafbcount;
	int srcfbcount = 1;		// last framebuffer is used as src frame
	RetCode ret;
	int enc_fbwidth, enc_fbheight, src_fbwidth, src_fbheight;
	int i;
	int enc_stride, src_stride;
	PhysicalAddress subSampBaseA = 0, subSampBaseB = 0;
	EncExtBufInfo extbufinfo =
	{ 0 };

	enc_fbwidth = (handle->params.enc_picwidth + 15) & ~15;
	enc_fbheight = (handle->params.enc_picheight + 15) & ~15;
	src_fbwidth = (handle->params.src_picwidth + 15) & ~15;
	src_fbheight = (handle->params.src_picheight + 15) & ~15;

	// mx6x only
	if (handle->mvc_extension)    // MVC
		extrafbcount = 2 + 2;    // Subsamp [2] + Subsamp MVC [2]
	else
		extrafbcount = 2;    // Subsamp buffer [2]

	handle->totalfb = handle->min_fb_count + extrafbcount + srcfbcount;
	handle->src_fbid = handle->totalfb - 1;

	handle->fb = calloc(handle->totalfb, sizeof(FrameBuffer));
	if (handle->fb == NULL)
	{
		printf("--- Failed to allocate FrameBuffer\n");
		return -1;
	}

	handle->pfbpool = calloc(handle->totalfb, sizeof(struct my_frame_buf *));
	if (handle->pfbpool == NULL)
	{
		printf("--- Failed to allocate my frame buffer\n");
		free(handle->fb);
		return -1;
	}

	// this is linear map type
	for (i = 0; i < handle->min_fb_count + extrafbcount; i++)
	{
		handle->pfbpool[i] = framebuf_alloc(handle, enc_fbwidth, enc_fbheight,
				0);
		if (handle->pfbpool[i] == NULL)
		{
			printf("--- framebuf %d alloc failed\n", i);
			goto err;
		}

		handle->fb[i].myIndex = i;
		handle->fb[i].bufY = handle->pfbpool[i]->addrY;
		handle->fb[i].bufCb = handle->pfbpool[i]->addrCb;
		handle->fb[i].bufCr = handle->pfbpool[i]->addrCr;
		handle->fb[i].strideY = handle->pfbpool[i]->strideY;
		handle->fb[i].strideC = handle->pfbpool[i]->strideC;
	}

	// mx6x only
	subSampBaseA = handle->fb[handle->min_fb_count].bufY;
	subSampBaseB = handle->fb[handle->min_fb_count + 1].bufY;
	if (handle->mvc_extension)
	{
		extbufinfo.subSampBaseAMvc = handle->fb[handle->min_fb_count + 2].bufY;
		extbufinfo.subSampBaseBMvc = handle->fb[handle->min_fb_count + 3].bufY;
	}

	// must be a multiple of 16
	enc_stride = (handle->params.enc_picwidth + 15) & ~15;
	src_stride = (handle->params.src_picwidth + 15) & ~15;

	ret = vpu_EncRegisterFrameBuffer(handle->vpu_enchandle, handle->fb,
			handle->min_fb_count, enc_stride, src_stride, subSampBaseA,
			subSampBaseB, &extbufinfo);
	if (ret != RETCODE_SUCCESS)
	{
		printf("--- Register frame buffer failed\n");
		goto err;
	}

	// allocate a single frame buffer for source frame
	handle->pfbpool[handle->src_fbid] = framebuf_alloc(handle, src_fbwidth,
			src_fbheight, 0);
	if (handle->pfbpool[handle->src_fbid] == NULL)
	{
		printf("--- failed to allocate single src framebuf\n");
		goto err;
	}

	handle->fb[handle->src_fbid].myIndex = handle->src_fbid;
	handle->fb[handle->src_fbid].bufY =
			handle->pfbpool[handle->src_fbid]->addrY;
	handle->fb[handle->src_fbid].bufCb =
			handle->pfbpool[handle->src_fbid]->addrCb;
	handle->fb[handle->src_fbid].bufCr =
			handle->pfbpool[handle->src_fbid]->addrCr;
	handle->fb[handle->src_fbid].strideY =
			handle->pfbpool[handle->src_fbid]->strideY;
	handle->fb[handle->src_fbid].strideC =
			handle->pfbpool[handle->src_fbid]->strideC;

	return 0;

	err: for (i = 0; i < handle->totalfb; i++)
	{
		framebuf_free(handle, handle->pfbpool[i]);
	}

	free(handle->fb);
	free(handle->pfbpool);
	return -1;
}

static void encoder_free_framebuffer(struct enc_handle *handle)
{
	int i;
	for (i = 0; i < handle->totalfb; ++i)
	{
		framebuf_free(handle, handle->pfbpool[i]);
	}

	free(handle->fb);
	free(handle->pfbpool);
}

static int get_initinfo(struct enc_handle *handle)
{
	RetCode ret;
	EncInitialInfo initinfo =
	{ 0 };

	ret = vpu_EncGetInitialInfo(handle->vpu_enchandle, &initinfo);
	if (ret != RETCODE_SUCCESS)
	{
		printf("--- Encoder GetInitialInfo failed\n");
		return -1;
	}

	handle->min_fb_count = initinfo.minFrameBufferCount;
	return 0;
}

static int prepare_encode(struct enc_handle *handle)
{
	int y_size;
	struct my_frame_buf *pfb = handle->pfbpool[handle->src_fbid];

	if (handle->params.src_picwidth != pfb->strideY)
	{
		printf("--- Make sure src pic width is a multiple of 16\n");
		return -1;
	}

	y_size = handle->params.src_picwidth * handle->params.src_picheight;

	// set image size and y address
	handle->yuv420_imgsize = y_size * 3 / 2;
	handle->y_addr = pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr;

	// set encode parameters
	CLEAR(handle->enc_param);
	handle->enc_param.sourceFrame = &handle->fb[handle->src_fbid];
	handle->enc_param.quantParam = 23;
	handle->enc_param.forceIPicture = 0;
	handle->enc_param.skipPicture = 0;
	handle->enc_param.enableAutoSkip = 1;
	handle->enc_param.encLeftOffset = 0;
	handle->enc_param.encTopOffset = 0;
	if ((handle->enc_param.encLeftOffset + handle->params.enc_picwidth)
			> handle->params.src_picwidth)
	{
		printf("--- Configure is failure for width and left offset\n");
		return -1;
	}
	if ((handle->enc_param.encTopOffset + handle->params.enc_picheight)
			> handle->params.src_picheight)
	{
		printf("--- Configure is failure for width and left offset\n");
		return -1;
	}

	return 0;
}

struct enc_handle *encode_open(struct enc_param param)
{
	struct enc_handle *handle = malloc(sizeof(struct enc_handle));
	if (!handle)
	{
		printf("--- malloc enc handle failed\n");
		return NULL;
	}

	CLEAR(*handle);
	handle->init_vpu = 1;    // initialize the VPU
	handle->quit = 0;
	handle->frame_counter = 0;
	handle->gop_offset_counter = 0;
	handle->yuv420_imgsize = 0;
	handle->y_addr = 0;
	handle->mvc_extension = 0;
	handle->params.src_picwidth = param.src_picwidth;
	handle->params.src_picheight = param.src_picheight;
	handle->params.enc_picwidth = param.enc_picwidth;
	handle->params.enc_picheight = param.enc_picheight;
	handle->params.fps = param.fps;
	handle->params.bitrate = param.bitrate;
	handle->params.gop = param.gop;
	handle->params.chroma_interleave = param.chroma_interleave;

	RetCode ret;

	// initialize VPU
	if (handle->init_vpu)
	{
		vpu_versioninfo ver;
		ret = vpu_Init(NULL);
		if (ret)
		{
			printf("--- VPU Init failure\n");
			return NULL;
		}

		ret = vpu_GetVersionInfo(&ver);
		if (ret)
		{
			printf("--- Cannot get version info, err: %d\n", ret);
			vpu_UnInit();
			return NULL;
		}

		printf("VPU firmware version: %d.%d.%d_r%d\n", ver.fw_major,
				ver.fw_minor, ver.fw_release, ver.fw_code);
		printf("VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor,
				ver.lib_release);
	}

	handle->mem_desc.size = STREAM_BUF_SIZE;
	ret = IOGetPhyMem(&handle->mem_desc);
	if (ret)
	{
		printf("--- Unable to obtain physical memory\n");
		return NULL;
	}

	handle->virt_bsbuf_addr = IOGetVirtMem(&handle->mem_desc);
	if (handle->virt_bsbuf_addr <= 0)
	{
		printf("--- Unable to map physical memory\n");
		IOFreePhyMem(&handle->mem_desc);
		return NULL;
	}

	handle->phy_bsbuf_addr = handle->mem_desc.phy_addr;

	// open encode instance
	CLEAR(handle->vpu_enchandle);

	EncOpenParam encop =
	{ 0 };

	encop.bitstreamBuffer = handle->phy_bsbuf_addr;
	encop.bitstreamBufferSize = STREAM_BUF_SIZE;
	encop.bitstreamFormat = STD_AVC;		// h264 only
	encop.mapType = 0;
	encop.linear2TiledEnable = 0;

	encop.picWidth = handle->params.enc_picwidth;
	encop.picHeight = handle->params.enc_picheight;

	encop.frameRateInfo = handle->params.fps;
	encop.bitRate = handle->params.bitrate;
	encop.gopSize = handle->params.gop;
	encop.slicemode.sliceMode = 0; /* 0: 1 slice per picture; 1: Multiple slices per picture */
	encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
	encop.slicemode.sliceSize = 4000; /* Size of a slice in bits or MB numbers */

	encop.initialDelay = 0;
	encop.vbvBufferSize = 0; /* 0 = ignore 8 */
	encop.intraRefresh = 0;
	encop.sliceReport = 0;
	encop.mbReport = 0;
	encop.mbQpReport = 0;
	encop.rcIntraQp = -1;
	encop.userQpMax = 0;
	encop.userQpMin = 0;
	encop.userQpMaxEnable = encop.userQpMinEnable = 0;

	encop.IntraCostWeight = 0;
	encop.MEUseZeroPmv = 0;
	/* (3: 16x16, 2:32x16, 1:64x32, 0:128x64, H.263(Short Header : always 3) */
	encop.MESearchRange = 3;

	encop.userGamma = (Uint32) (0.75 * 32768); /*  (0*32768 <= gamma <= 1*32768) */
	encop.RcIntervalMode = 1; /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
	encop.MbInterval = 0;
	encop.avcIntra16x16OnlyModeEnable = 0;

	encop.ringBufferEnable = 0;
	encop.dynamicAllocEnable = 0;
	encop.chromaInterleave = handle->params.chroma_interleave;

	// STD_AVC only params
	encop.EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
	encop.EncStdParam.avcParam.avc_disableDeblk = 0;
	encop.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
	encop.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;
	encop.EncStdParam.avcParam.avc_chromaQpOffset = 10;
	encop.EncStdParam.avcParam.avc_audEnable = 0;

	// mx6x only
	encop.EncStdParam.avcParam.interview_en = 0;
	encop.EncStdParam.avcParam.paraset_refresh_en = 0;
	encop.EncStdParam.avcParam.prefix_nal_en = 0;
	encop.EncStdParam.avcParam.mvc_extension = handle->mvc_extension;
	encop.EncStdParam.avcParam.avc_frameCroppingFlag = 0;
	encop.EncStdParam.avcParam.avc_frameCropLeft = 0;
	encop.EncStdParam.avcParam.avc_frameCropRight = 0;
	encop.EncStdParam.avcParam.avc_frameCropTop = 0;
	encop.EncStdParam.avcParam.avc_frameCropBottom = 0;

	ret = vpu_EncOpen(&handle->vpu_enchandle, &encop);
	if (ret != RETCODE_SUCCESS)
	{
		printf("--- Encoder open failed: %d\n", ret);
		goto err1;
	}

	ret = get_initinfo(handle);
	if (ret < 0)
	{
		printf("--- encode get initinfo failed\n");
		goto err2;
	}

	init_framebuf(handle);
	ret = encoder_allocate_framebuffer(handle);
	if (ret < 0)
	{
		printf("--- encode allocate framebuffer failed\n");
		goto err2;
	}

	ret = prepare_encode(handle);
	if (ret < 0)
	{
		printf("--- encode prepare_encode failed\n");
		goto err2;
	}

	printf("+++ Encode Opened\n");
	return handle;

	err2: vpu_EncClose(handle->vpu_enchandle);
	err1: free(handle);
	return NULL;
}

void encode_close(struct enc_handle *handle)
{
	RetCode ret;

	encoder_free_framebuffer(handle);

	ret = vpu_EncClose(handle->vpu_enchandle);
	if (ret == RETCODE_FRAME_NOT_COMPLETE)
	{
		vpu_SWReset(handle->vpu_enchandle, 0);
		vpu_EncClose(handle->vpu_enchandle);
	}

	IOFreeVirtMem(&handle->mem_desc);
	IOFreePhyMem(&handle->mem_desc);

	if (handle->init_vpu)
		vpu_UnInit();

	free(handle);
	handle = NULL;
	printf("+++ Encode Closed\n");
}

int encode_get_headers(struct enc_handle *handle, void **buf, int *len,
		enum pic_t *type)
{
	if (handle->gop_offset_counter != 0)	// no need for headers
		return 0;

	EncHeaderParam enchdr_param =
	{ 0 };
	U32 vbuf;
	static int next_stage = 0;    // 0: sps, 1: msps, 2: pps, 3: mpps
	static int stage_end = 0;

	if (stage_end == 1)    // reset
	{
		stage_end = 0;
		return 0;
	}

	// H264
	// SPS
	// disable this only if (mvc_extension == 1 && mvc_paraset_refresh_en == 1)
	if (next_stage == 0)		// SPS
	{
		enchdr_param.headerType = SPS_RBSP;
		vpu_EncGiveCommand(handle->vpu_enchandle, ENC_PUT_AVC_HEADER,
				&enchdr_param);

		vbuf = enchdr_param.buf + handle->virt_bsbuf_addr
				- handle->phy_bsbuf_addr;
		*buf = (void *) vbuf;
		*len = enchdr_param.size;
		*type = SPS;

		if (handle->mvc_extension)
			next_stage = 1;
		else
			next_stage = 2;
	}
	else if (next_stage == 1)    // MVC SPS
	{
		enchdr_param.headerType = SPS_RBSP_MVC;
		vpu_EncGiveCommand(handle->vpu_enchandle, ENC_PUT_AVC_HEADER,
				&enchdr_param);

		vbuf = enchdr_param.buf + handle->virt_bsbuf_addr
				- handle->phy_bsbuf_addr;
		*buf = (void *) vbuf;
		*len = enchdr_param.size;
		*type = SPS;

		next_stage = 2;
	}
	else if (next_stage == 2)    // PPS
	{
		enchdr_param.headerType = PPS_RBSP;
		vpu_EncGiveCommand(handle->vpu_enchandle, ENC_PUT_AVC_HEADER,
				&enchdr_param);

		vbuf = enchdr_param.buf + handle->virt_bsbuf_addr
				- handle->phy_bsbuf_addr;
		*buf = (void *) vbuf;
		*len = enchdr_param.size;
		*type = PPS;

		if (handle->mvc_extension)
			next_stage = 3;
		else
		{
			next_stage = 0;
			stage_end = 1;    // a circle complete
		}
	}
	else if (next_stage == 3)    // MVC PPS
	{
		enchdr_param.headerType = PPS_RBSP_MVC;
		vpu_EncGiveCommand(handle->vpu_enchandle, ENC_PUT_AVC_HEADER,
				&enchdr_param);

		vbuf = enchdr_param.buf + handle->virt_bsbuf_addr
				- handle->phy_bsbuf_addr;
		*buf = (void *) vbuf;
		*len = enchdr_param.size;
		*type = PPS;

		next_stage = 0;
		stage_end = 1;    // a circle complete
	}

	return 1;
}

static int encode_fill_data(struct enc_handle *handle, void *buf, int len)
{
	struct my_frame_buf *pfb = handle->pfbpool[handle->src_fbid];

	if ((handle->yuv420_imgsize != pfb->desc.size)
			|| (len < handle->yuv420_imgsize))
	{
		printf(
				"--- %s:%s yuv420_imgsize != desc.size or len < handle->yuv420_imgsize\n",
				__FILE__, __FUNCTION__);
		abort();
	}
	memcpy((void *) handle->y_addr, buf, handle->yuv420_imgsize);

	return handle->yuv420_imgsize;
}

int encode_do(struct enc_handle *handle, void *ibuf, int ilen, void **pobuf,
		int *polen, enum pic_t *type)
{
	EncOutputInfo outinfo =
	{ 0 };
	RetCode ret = 0;
	int loop_id;
	enum pic_t pictp = NONE;

	ret = encode_fill_data(handle, ibuf, ilen);
	if (ret < 0)
		return -1;

	ret = vpu_EncStartOneFrame(handle->vpu_enchandle, &handle->enc_param);
	handle->enc_param.forceIPicture = 0;		// reset forced IPic
	if (ret != RETCODE_SUCCESS)
	{
		printf("--- vpu_EncStartOneFrame failed Err code: %d\n", ret);
		return -1;
	}

	// get output
	loop_id = 0;
	while (vpu_IsBusy())
	{
		vpu_WaitForInt(200);

		if (loop_id == 20)
		{
			printf("--- timeout exceeds 20 times\n");
			ret = vpu_SWReset(handle->vpu_enchandle, 0);
			return -1;
		}
		loop_id++;
	}

	ret = vpu_EncGetOutputInfo(handle->vpu_enchandle, &outinfo);
	if (ret != RETCODE_SUCCESS)
	{
		printf("--- vpu_EncGetOutputInfo failed Err code: %d\n", ret);
		return -1;
	}

	if (outinfo.skipEncoded)
		printf("!!! Skip encoding one Frame\n");

	switch (outinfo.picType)
	{
		case 0:		// I picture
			pictp = I;
			break;
		case 1:		// P picture
			pictp = P;
			break;
		case 3:		// B picture
			pictp = B;
			break;
		default:	// BI, SKIPPED etc
			pictp = NONE;
			break;
	}

	U32 vbuf;
	vbuf = outinfo.bitstreamBuffer + handle->virt_bsbuf_addr
			- handle->phy_bsbuf_addr;

	*pobuf = (void *) vbuf;
	*polen = outinfo.bitstreamSize;
	*type = pictp;

	handle->frame_counter++;
	if (++handle->gop_offset_counter >= handle->params.gop)
		handle->gop_offset_counter = 0;

	return 0;
}

int encode_set_qp(struct enc_handle *handle, int val)
{
	int valid = 23;

	// check valid
	if (val > 51)
	{
		printf("!!! H264 QP is greater than 51!\n");
		valid = 51;
	}
	else if (val < 0)
	{
		printf("!!! H264 QP is less than 0!\n");
		valid = 0;
	}
	else
	{
		valid = val;
	}

	handle->enc_param.quantParam = valid;

	return 0;
}

void encode_force_Ipic(struct enc_handle *handle)
{
	handle->enc_param.forceIPicture = 1;
	handle->gop_offset_counter = 0;		// recount gop
}

int encode_set_gop(struct enc_handle *handle, int val)
{
	RetCode ret;
	ret = vpu_EncGiveCommand(handle->vpu_enchandle, ENC_SET_GOP_NUMBER, &val);
	if (ret != RETCODE_SUCCESS)
		return -1;

	handle->params.gop = val;    // save gop
	return 0;
}

int encode_set_bitrate(struct enc_handle *handle, int val)
{
	RetCode ret;
	ret = vpu_EncGiveCommand(handle->vpu_enchandle, ENC_SET_BITRATE, &val);
	if (ret != RETCODE_SUCCESS)
		return -1;

	handle->params.bitrate = val;
	return 0;
}

int encode_set_framerate(struct enc_handle *handle, int val)
{
	RetCode ret;
	ret = vpu_EncGiveCommand(handle->vpu_enchandle, ENC_SET_FRAME_RATE, &val);
	if (ret != RETCODE_SUCCESS)
		return -1;

	handle->params.fps = val;
	return 0;
}
