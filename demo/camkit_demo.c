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
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include "camkit.h"

#define SEPARATOR_BYTES 8
const uint8_t SEPARATOR[SEPARATOR_BYTES] =
{
    0x53, 0x45, 0x50, 0x41, 0x52, 0x41, 0x54, 0x45
};   // "separate", 8 Bytes
FILE *outfd = NULL;
int quit;
int debug = 0;

void quit_func(int sig)
{
	quit = 1;
}

void display_usage(void)
{
	printf("Usage: capenc_demo [options]\n");
	printf("-? help\n");
	printf("-d debug on\n");
	printf(
			"-s 0/1/2/3 set stage, 0: capture only, 1: capture + convert, 2: capture + encode 3: capture + convert + encode (3)\n");
	printf("-i video device, (\"/dev/video0\")\n");
	printf("-o h264 output file (no output)\n");
	printf("-c capture pixel format 0:YUYV, 1:YUV420 (YUYV)\n");
	printf("-w width (640)\n");
	printf("-h height (480)\n");
	printf("-r bitrate kbps (4000)\n");
	printf("-f fps (15)\n");
	printf("-t chroma interleaved (0)\n");
	printf("-g size of group of pictures (12)\n");
}

int main(int argc, char *argv[])
{
	struct cap_handle *caphandle = NULL;
	struct cvt_handle *cvthandle = NULL;
	struct enc_handle *enchandle = NULL;
	struct cap_param capp;
	struct cvt_param ipup;
	struct enc_param vpup;
	unsigned long count = 0;
	int stage = 0b00000011;

	U32 vfmt = V4L2_PIX_FMT_YUYV;
	U32 ofmt = V4L2_PIX_FMT_YUV420;

	// set default values
	capp.dev_name = "/dev/video0";
	capp.width = 640;
	capp.height = 480;
	capp.pixfmt = vfmt;
	capp.rate = 15;

	ipup.inwidth = 640;
	ipup.inheight = 480;
	ipup.inpixfmt = vfmt;
	ipup.outwidth = 640;
	ipup.outheight = 480;
	ipup.outpixfmt = ofmt;

	vpup.src_picwidth = 640;
	vpup.src_picheight = 480;
	vpup.enc_picwidth = 640;
	vpup.enc_picheight = 480;
	vpup.chroma_interleave = 0;
	vpup.fps = 15;
	vpup.gop = 12;
	vpup.bitrate = 4000;

	char *outfile = NULL;
	// options
	int opt = 0;
	static const char *optString = "?di:o:w:h:r:f:t:g:s:c:";

	opt = getopt(argc, argv, optString);
	while (opt != -1)
	{
		int fmt;
		switch (opt)
		{
			case '?':
				display_usage();
				return 0;
			case 'd':
				debug = 1;
				break;
			case 's':
				stage = atoi(optarg);
				if (stage < 0 || stage > 3)
					stage = 3;
				break;
			case 'i':
				capp.dev_name = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'c':
				fmt = atoi(optarg);
				if (fmt == 1)
					capp.pixfmt = ipup.inpixfmt = V4L2_PIX_FMT_YUV420;
				else
					capp.pixfmt = ipup.inpixfmt = V4L2_PIX_FMT_YUYV;
				break;
			case 'w':
				capp.width = ipup.inwidth = ipup.outwidth = vpup.src_picwidth =
						vpup.enc_picwidth = atoi(optarg);
				break;
			case 'h':
				capp.height = ipup.inheight = ipup.outheight =
						vpup.src_picheight = vpup.enc_picheight = atoi(optarg);
				break;
			case 'r':
				vpup.bitrate = atoi(optarg);
				break;
			case 'f':
				capp.rate = vpup.fps = atoi(optarg);
				break;
			case 't':
				vpup.chroma_interleave = atoi(optarg);
				break;
			case 'g':
				vpup.gop = atoi(optarg);
				break;
			default:
				printf("Unknown option: %s\n", optarg);
				display_usage();
				return -1;
		}

		opt = getopt(argc, argv, optString);
	}

	if (outfile)
		outfd = fopen(outfile, "wb");

	signal(SIGINT, quit_func);

	caphandle = capture_open(capp);
	if (!caphandle)
	{
		printf("--- Open capture failed\n");
		return -1;
	}

	if ((stage & 0b00000001) != 0)
	{
		cvthandle = convert_open(ipup);
		if (!cvthandle)
		{
			printf("--- Open convert failed\n");
			return -1;
		}
	}

	if ((stage & 0b00000010) != 0)
	{
		enchandle = encode_open(vpup);
		if (!enchandle)
		{
			printf("--- Open encode failed\n");
			return -1;
		}
	}
	// start capture encode loop
	int ret;
	void *cap_buf, *cvt_buf, *hd_buf, *enc_buf;
	int cap_len, cvt_len, hd_len, enc_len;
	enum pic_t ptype;
	struct timeval ctime, ltime;
	unsigned long fps_counter = 0;
	int sec, usec;
	double stat_time = 0;

	capture_start(caphandle);		// !!! need to start capture stream!
	quit = 0;
	gettimeofday(&ltime, NULL);
	while (!quit)
	{
		if (debug)		// fps stuff
		{
			gettimeofday(&ctime, NULL);
			sec = ctime.tv_sec - ltime.tv_sec;
			usec = ctime.tv_usec - ltime.tv_usec;
			if (usec < 0)
			{
				sec--;
				usec = usec + 1000000;
			}
			stat_time = (sec * 1000000) + usec;		// diff in microsecond

			if (stat_time >= 1000000)    // >= 1s
			{
				printf("********* FPS: %ld\n", fps_counter);

				fps_counter = 0;
				ltime = ctime;
			}
			fps_counter++;
		}

		ret = capture_get_data(caphandle, &cap_buf, &cap_len);
		if (ret != 0)
		{
			if (ret < 0)		// error
			{
				printf("--- capture_get_data failed\n");
				break;
			}
			else	// again
			{
				usleep(10000);
				continue;
			}
		}
		if (cap_len <= 0)
		{
			printf("!!! No capture data\n");
			continue;
		}
		// else

		if (debug)
			fputc('.', stdout);

		if ((stage & 0b00000011) == 0)    // no convert, no encode, capture only
		{
			if (outfd)
				fwrite(cap_buf, 1, cap_len, outfd);

			fflush(stdout);
			if (count++ > 100)
			{
				printf("\n");
				count = 0;
			}
			continue;
		}

		if ((stage & 0b00000001) == 0)    // no convert
		{
			cvt_buf = cap_buf;
			cvt_len = cap_len;
		}
		else	// do convert
		{
			ret = convert_do(cvthandle, cap_buf, cap_len, &cvt_buf, &cvt_len);
			if (ret < 0)
			{
				printf("--- convert_do failed\n");
				break;
			}
			if (cvt_len <= 0)
			{
				printf("!!! No convert data\n");
				continue;
			}
			// else

			if (debug)
				fputc('_', stdout);
		}

		if ((stage & 0b00000010) == 0)		// no encode
		{
			if (outfd)
				fwrite(cvt_buf, 1, cvt_len, outfd);

			fflush(stdout);
			if (count++ > 100)
			{
				printf("\n");
				count = 0;
			}
			continue;
		}

		// fetch h264 headers first!
		while ((ret = encode_get_headers(enchandle, &hd_buf, &hd_len, &ptype))
				!= 0)
		{
			if (outfd)
				fwrite(hd_buf, 1, hd_len, outfd);

			if (debug)
				fputc('S', stdout);
		}

		ret = encode_do(enchandle, cvt_buf, cvt_len, &enc_buf, &enc_len,
				&ptype);
		if (ret < 0)
		{
			printf("--- encode_do failed\n");
			break;
		}
		if (enc_len <= 0)
		{
			printf("!!! No encode data\n");
			continue;
		}
		// else

		if (outfd)
			fwrite(enc_buf, 1, enc_len, outfd);

		if (debug)
		{
			char c;
			switch (ptype)
			{
				case PPS:
					c = 'S';
					break;
				case SPS:
					c = 'S';
					break;
				case I:
					c = 'I';
					break;
				case P:
					c = 'P';
					break;
				case B:
					c = 'B';
					break;
				default:
					c = 'N';
					break;
			}
			fputc(c, stdout);
			fflush(stdout);

			if (count++ > 100)
			{
				printf("\n");
				count = 0;
			}
		}
	}
	capture_stop(caphandle);

	if ((stage & 0b00000010) != 0)
		encode_close(enchandle);
	if ((stage & 0b00000001) != 0)
		convert_close(cvthandle);
	capture_close(caphandle);

	if (outfd)
		fclose(outfd);
	return 0;
}
