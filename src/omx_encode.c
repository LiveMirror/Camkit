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
#include <string.h>
#include <assert.h>
#include <bcm_host.h>
#include <ilclient.h>
#include "camkit/encode.h"

#define OMX_VIDENC_INPUT_PORT 200
#define OMX_VIDENC_OUTPUT_PORT 201

#define MAX_SIZE_COMBINATION_BUFFER	(128 * 1024)	// 128k is enough

struct enc_handle
{
	COMPONENT_T *video_encode;
	COMPONENT_T *list[5];
	ILCLIENT_T *client;
	OMX_BUFFERHEADERTYPE *out;
	void *combination_buffer;
	unsigned long combination_buffer_ptr;
	unsigned long frame_counter;

	struct enc_param params;
};

static void print_def(OMX_PARAM_PORTDEFINITIONTYPE def)
{
	printf("+++ Def - Port %u: %s %u/%u %u %u %s,%s,%s %ux%u %ux%u @%u %u\n",
			def.nPortIndex, def.eDir == OMX_DirInput ? "in" : "out",
			def.nBufferCountActual, def.nBufferCountMin, def.nBufferSize,
			def.nBufferAlignment, def.bEnabled ? "enabled" : "disabled",
			def.bPopulated ? "populated" : "not pop.",
			def.bBuffersContiguous ? "contig." : "not cont.",
			def.format.video.nFrameWidth, def.format.video.nFrameHeight,
			def.format.video.nStride, def.format.video.nSliceHeight,
			def.format.video.xFramerate, def.format.video.eColorFormat);
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
	handle->combination_buffer = malloc(MAX_SIZE_COMBINATION_BUFFER);
	if (!handle->combination_buffer)
	{
		printf("--- Failed to malloc combination buffer of size: %d", MAX_SIZE_COMBINATION_BUFFER);
		goto err0;
	}
	handle->combination_buffer_ptr = 0;
	handle->video_encode = NULL;
	memset(handle->list, 0, sizeof(handle->list));
	handle->client = NULL;
	handle->out = NULL;
	handle->frame_counter = 0;
	handle->params.src_picwidth = param.src_picwidth;
	handle->params.src_picheight = param.src_picheight;
	handle->params.enc_picwidth = param.enc_picwidth;
	handle->params.enc_picheight = param.enc_picheight;
	handle->params.fps = param.fps;
	handle->params.bitrate = param.bitrate;
	handle->params.gop = param.gop;
	handle->params.chroma_interleave = param.chroma_interleave;

	bcm_host_init();

	if ((handle->client = ilclient_init()) == NULL)
	{
		printf("--- ilclient_init failed\n");
		goto err0;
	}

	if (OMX_Init() != OMX_ErrorNone)
	{
		printf("--- OMX_Init failed\n");
		goto err1;
	}

	// create video encoder
	OMX_ERRORTYPE ret;
	ret = ilclient_create_component(handle->client, &handle->video_encode,
			"video_encode",
			ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS
					| ILCLIENT_ENABLE_OUTPUT_BUFFERS);
	if (ret != 0)
	{
		printf("--- ilclient_create_component failed\n");
		goto err2;
	}
	handle->list[0] = handle->video_encode;

	// get current settings of video_encode component from input port
	OMX_PARAM_PORTDEFINITIONTYPE def;
	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = OMX_VIDENC_INPUT_PORT;

	if (OMX_GetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamPortDefinition, &def) != OMX_ErrorNone)
	{
		printf("--- OMX_GetParameter for video encode input port failed\n");
		goto err3;
	}

	print_def(def);

	def.format.video.nFrameWidth = handle->params.src_picwidth;
	def.format.video.nFrameHeight = handle->params.src_picheight;
	def.format.video.xFramerate = handle->params.fps << 16;
	def.format.video.nSliceHeight = def.format.video.nFrameHeight;
	def.format.video.nStride = def.format.video.nFrameWidth;
	def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

	print_def(def);

	ret = OMX_SetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamPortDefinition, &def);
	if (ret != OMX_ErrorNone)
	{
		printf(
				"--- OMX_SetParameter for video encode input port failed with %x\n",
				ret);
		goto err3;
	}

	// set output format
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;

	ret = OMX_SetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoPortFormat, &format);
	if (ret != OMX_ErrorNone)
	{
		printf(
				"--- OMX_SetParameter for video encode output port failed with: %x\n",
				ret);
		goto err3;
	}

	// set output bitrate
	OMX_VIDEO_PARAM_BITRATETYPE bitrateType;
	memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
	bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
	bitrateType.nVersion.nVersion = OMX_VERSION;
	bitrateType.eControlRate = OMX_Video_ControlRateVariable;
	bitrateType.nTargetBitrate = handle->params.bitrate * 1000;    // to kbps
	bitrateType.nPortIndex = OMX_VIDENC_OUTPUT_PORT;

	ret = OMX_SetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoBitrate, &bitrateType);
	if (ret != OMX_ErrorNone)
	{
		printf(
				"--- OMX_SetParameter for bitrate for video encode output port failed with %x\n",
				ret);
		goto err3;
	}

	// get current bitrate
	memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
	bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
	bitrateType.nVersion.nVersion = OMX_VERSION;
	bitrateType.nPortIndex = OMX_VIDENC_OUTPUT_PORT;

	if (OMX_GetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoBitrate, &bitrateType) != OMX_ErrorNone)
	{
		printf(
				"%s:%d: OMX_GetParameter() for video_encode for bitrate output port failed!\n",
				__FUNCTION__, __LINE__);
		goto err3;
	}
	printf("+++ Bitrate set to %u\n", bitrateType.nTargetBitrate);

	// set SPS/PPS on each I-frame
	OMX_CONFIG_PORTBOOLEANTYPE inlineHeader;
	memset(&inlineHeader, 0, sizeof(OMX_CONFIG_PORTBOOLEANTYPE));
	inlineHeader.nSize = sizeof(OMX_CONFIG_PORTBOOLEANTYPE);
	inlineHeader.nVersion.nVersion = OMX_VERSION;
	inlineHeader.bEnabled = OMX_TRUE;    // Enable SPS/PPS insertion in the encoder bitstream
	inlineHeader.nPortIndex = OMX_VIDENC_OUTPUT_PORT;

	ret = OMX_SetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &inlineHeader);
	if (ret != OMX_ErrorNone)
	{
		printf("--- OMX_SetParameter for inline heander failed\n");
		goto err3;
	}

	// get current settings of avcConfig
	OMX_VIDEO_PARAM_AVCTYPE avcConfig;
	memset(&avcConfig, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
	avcConfig.nSize = sizeof(OMX_VIDEO_PARAM_AVCTYPE);
	avcConfig.nVersion.nVersion = OMX_VERSION;
	avcConfig.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
	if (OMX_GetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoAvc, &avcConfig) != OMX_ErrorNone)
	{
		printf("--- %s:%d OMX_GetParameter for avcConfig failed\n",
				__FUNCTION__, __LINE__);
		goto err3;
	}

	// set new avcConfig
	avcConfig.nPFrames = handle->params.gop - 1;
	ret = OMX_SetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoAvc, &avcConfig);
	if (ret != OMX_ErrorNone)
	{
		printf("--- OMX_SetParameter for avcConfig failed\n");
		goto err3;
	}

	// get and check avcConfig again
	memset(&avcConfig, 0, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
	avcConfig.nSize = sizeof(OMX_VIDEO_PARAM_AVCTYPE);
	avcConfig.nVersion.nVersion = OMX_VERSION;
	avcConfig.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
	if (OMX_GetParameter(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexParamVideoAvc, &avcConfig) != OMX_ErrorNone)
	{
		printf("--- %s:%d OMX_GetParameter for avcConfig failed\n",
				__FUNCTION__, __LINE__);
		goto err3;
	}
	printf("+++ nPFrames set to: %u\n", avcConfig.nPFrames);

	// change il state
	if (ilclient_change_component_state(handle->video_encode, OMX_StateIdle)
			== -1)
	{
		printf(
				"--- %s:%d: ilclient_change_component_state(video_encode, OMX_StateIdle) failed",
				__FUNCTION__, __LINE__);
		goto err3;
	}

	if (ilclient_enable_port_buffers(handle->video_encode,
	OMX_VIDENC_INPUT_PORT, NULL, NULL,
	NULL) != 0)
	{
		printf("--- enabling port buffers for input failed!\n");
		goto err4;
	}

	if (ilclient_enable_port_buffers(handle->video_encode,
	OMX_VIDENC_OUTPUT_PORT, NULL, NULL,
	NULL) != 0)
	{
		printf("--- enabling port buffers for output failed!\n");
		goto err5;
	}

	if (ilclient_change_component_state(handle->video_encode,
			OMX_StateExecuting) == -1)
	{
		printf(
				"--- %s:%d: ilclient_change_component_state(video_encode, OMX_StateExecuting) failed",
				__FUNCTION__, __LINE__);
		goto err6;
	}

	printf("+++ Encode Opened\n");
	return handle;

	err6: ilclient_disable_port_buffers(handle->video_encode,
	OMX_VIDENC_OUTPUT_PORT, NULL, NULL,
	NULL);
	err5: ilclient_disable_port_buffers(handle->video_encode,
	OMX_VIDENC_INPUT_PORT, NULL, NULL,
	NULL);
	err4: ilclient_state_transition(handle->list, OMX_StateIdle);
	ilclient_state_transition(handle->list, OMX_StateLoaded);
	err3: ilclient_cleanup_components(handle->list);
	err2: OMX_Deinit();
	err1: ilclient_destroy(handle->client);
	err0: free(handle);
	return NULL;
}

void encode_close(struct enc_handle *handle)
{
	ilclient_disable_port_buffers(handle->video_encode, OMX_VIDENC_INPUT_PORT,
	NULL, NULL, NULL);
	ilclient_disable_port_buffers(handle->video_encode, OMX_VIDENC_OUTPUT_PORT,
	NULL, NULL, NULL);
	ilclient_state_transition(handle->list, OMX_StateIdle);
	ilclient_state_transition(handle->list, OMX_StateLoaded);
	ilclient_cleanup_components(handle->list);
	OMX_Deinit();
	ilclient_destroy(handle->client);
	free(handle->combination_buffer);
	free(handle);
	printf("+++ Encode Closed\n");
}

int encode_do(struct enc_handle *handle, void *ibuf, int ilen, void **pobuf,
		int *polen, enum pic_t *type)
{
	*pobuf = NULL;
	*polen = 0;
	*type = NONE;

	OMX_BUFFERHEADERTYPE *buf;
	buf = ilclient_get_input_buffer(handle->video_encode, OMX_VIDENC_INPUT_PORT,
			0);  // 0:non-block, 1:block, set to non-block to avoid freezing after long time encode

	if (buf != NULL)    // emtpy this buffer only when the buf is not NULL
	{
		memcpy(buf->pBuffer, ibuf, ilen);
		buf->nFilledLen = ilen;

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(handle->video_encode), buf)
				!= OMX_ErrorNone)
		{
			printf("--- Failed to empty ilclient input buffer\n");
			return -1;
		}
	}

    // no matter buf above is NULL or not, call get output buffer
	handle->out = ilclient_get_output_buffer(handle->video_encode,
	OMX_VIDENC_OUTPUT_PORT, 1);    // block
	if (handle->out == NULL)    // shouldn't equal NULL, since it's block getting
	{
		printf("--- Failed to get ilclient output buffer\n");
		return -1;
	}
        
	OMX_ERRORTYPE ret = OMX_FillThisBuffer(ILC_GET_HANDLE(handle->video_encode),
			handle->out);
	if (ret != OMX_ErrorNone)
	{
		printf("--- Error filling ilclient output buffer: %x\n", ret);
		return -1;
	}

	OMX_U32 flag = handle->out->nFlags;
	if ((flag == 0) &&
			(handle->out->nFilledLen == 0)) // sometimes there are partial frames that have flag == 0 but len != 0, need to exclude these cases
	{
		// output nothing
		// not an error, this means need more data for encoding?
	}
	else if (!(flag & OMX_BUFFERFLAG_CODECCONFIG) &&
			(flag & OMX_BUFFERFLAG_ENDOFFRAME))	// it's a whole I/B/P frame
	{
		*pobuf = handle->out->pBuffer;
		*polen = handle->out->nFilledLen;
		*type = NONE;    // TODO: how to get the type?
		handle->frame_counter++;
	}
	else	// PPS/SPS or part frame, combination is needed
	{
		// SPS/PPS will be combined with I-frame, part frame will
		// be combined to whole
		handle->combination_buffer_ptr = 0;
		// copy the first part
		memcpy(handle->combination_buffer + 0, handle->out->pBuffer, handle->out->nFilledLen);
		handle->combination_buffer_ptr += handle->out->nFilledLen;

		while (1)
		{
			OMX_ERRORTYPE ret = OMX_FillThisBuffer(ILC_GET_HANDLE(handle->video_encode),
					handle->out);
			if (ret != OMX_ErrorNone)
			{
				printf("--- Error filling ilclient output buffer: %x\n", ret);
				return -1;
			}

			if ((handle->out->nFlags == 0) &&
					(handle->out->nFilledLen == 0))		// break if output nothing
				break;

			// copy the remaining part
			memcpy(handle->combination_buffer + handle->combination_buffer_ptr,
					handle->out->pBuffer, handle->out->nFilledLen);
			handle->combination_buffer_ptr += handle->out->nFilledLen;

			flag = handle->out->nFlags;	// get new flag
			if ((flag & OMX_BUFFERFLAG_ENDOFFRAME)
					&& !(flag & OMX_BUFFERFLAG_CODECCONFIG))	// finish if it's end of frame and not SPS/PPS, SPS/PPS will continue to be combined with I-frame
				break;

			handle->out->nFlags = handle->out->nFilledLen = 0;	// continue with cleared flag and len
		}

		*pobuf = handle->combination_buffer;
		*polen = handle->combination_buffer_ptr;
		*type = NONE;    // TODO: how to get the type?
		handle->frame_counter++;
	}

	handle->out->nFilledLen = 0;    // set to 0 at end
	return 0;
}

int encode_get_headers(struct enc_handle *handle, void **pbuf, int *plen,
		enum pic_t *type)
{
	// SPS/PPS is combined with I-frame in encode_do(), here is just a placeholder
    UNUSED(handle);

	*pbuf = NULL;
	*plen = 0;
	*type = NONE;

	return 0;
}

int encode_set_qp(struct enc_handle *handle, int val)
{
	UNUSED(handle);
	UNUSED(val);
	printf("*** %s.%s: This function is not implemented\n", __FILE__,
			__FUNCTION__);
	return -1;
}

int encode_set_gop(struct enc_handle *handle, int val)
{
	UNUSED(handle);
	UNUSED(val);
	printf("*** %s.%s: This function is not implemented\n", __FILE__,
			__FUNCTION__);
	return -1;
}

int encode_set_bitrate(struct enc_handle *handle, int val)
{
	OMX_VIDEO_CONFIG_BITRATETYPE tbitrate;
	memset(&tbitrate, 0, sizeof(OMX_VIDEO_CONFIG_BITRATETYPE));
	tbitrate.nSize = sizeof(OMX_VIDEO_CONFIG_BITRATETYPE);
	tbitrate.nVersion.nVersion = OMX_VERSION;
	tbitrate.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
	tbitrate.nEncodeBitrate = val * 1000;

	OMX_ERRORTYPE ret = OMX_SetConfig(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexConfigVideoBitrate, &tbitrate);

	if (ret != OMX_ErrorNone)
	{
		printf("--- failed to set bitrate to %d kbps\n", val);
		return -1;
	}

	printf("+++ Set bitrate to %d kbps\n", val);

	return 0;
}

int encode_set_framerate(struct enc_handle *handle, int val)
{
	OMX_CONFIG_FRAMERATETYPE framerate;
	memset(&framerate, 0, sizeof(OMX_CONFIG_FRAMERATETYPE));
	framerate.nSize = sizeof(OMX_CONFIG_FRAMERATETYPE);
	framerate.nVersion.nVersion = OMX_VERSION;
	framerate.nPortIndex = OMX_VIDENC_OUTPUT_PORT;
	framerate.xEncodeFramerate = val;

	OMX_ERRORTYPE ret = OMX_SetConfig(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexConfigVideoFramerate, &framerate);
	if (ret != OMX_ErrorNone)
	{
		printf("--- failed to set framerate to %d\n", val);
		return -1;
	}

	printf("--- Set framerate to %d\n", val);
	return 0;
}

void encode_force_Ipic(struct enc_handle *handle)
{
	UNUSED(handle);

	OMX_CONFIG_BOOLEANTYPE requestIFrame;
	memset(&requestIFrame, 0, sizeof(OMX_CONFIG_BOOLEANTYPE));
	requestIFrame.nSize = sizeof(OMX_CONFIG_BOOLEANTYPE);
	requestIFrame.nVersion.nVersion = OMX_VERSION;
	requestIFrame.bEnabled = OMX_TRUE;		// this automatically resets itself.

	OMX_ERRORTYPE ret = OMX_SetConfig(ILC_GET_HANDLE(handle->video_encode),
			OMX_IndexConfigBrcmVideoRequestIFrame, &requestIFrame);
	if (ret != OMX_ErrorNone)
	{
		printf("--- failed to force IPic\n");
	}

	return;
}
