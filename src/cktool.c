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
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include "camkit.h"

FILE *outfd = NULL;
int quit = 0;
int debug = 0;

static void quit_func(int sig)
{
	quit = 1;
}

static void display_usage(void)
{
	printf("Usage: #cktool [options]\n");
	printf("-? help\n");
	printf("-v version\n");
	printf("-d debug on\n");
	printf("-s 0/1/3/7/15 set stage, "
			"\n\t0: capture only"
			"\n\t1: capture + convert"
			"\n\t3: capture + convert + encode (default)"
			"\n\t7: capture + convert + encode + pack"
			"\n\t15: capture + convert + encode + pack + network\n");
	printf("-i video device, (\"/dev/video0\")\n");
	printf("-o dump to file (no dump)\n");
	printf("-a ip address of stream server (none)\n");
	printf("-p port of stream server (none)\n");
	printf("-c capture pixel format 0:YUYV, 1:YUV420 (YUYV)\n");
	printf("-w width (640)\n");
	printf("-h height (480)\n");
	printf("-r bitrate kbps (1000)\n");
	printf("-f fps (15)\n");
	printf("-t chroma interleaved (0)\n");
	printf("-g size of group of pictures (12)\n");
}

static void display_version(void)
{
	printf("CAMKIT - CAMera toolKIT\n");
	printf("Version: %s, built on %s %s\n", version, __TIME__, __DATE__);
	printf("Project homepage: http://git.oschina.net/andyspider/Camkit\n");
	printf("Report bugs to AndyHuang (andy@andy87.com)\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	struct cap_handle *caphandle = NULL;
	struct cvt_handle *cvthandle = NULL;
	struct enc_handle *enchandle = NULL;
	struct pac_handle *pachandle = NULL;
	struct net_handle *nethandle = NULL;
	struct tms_handle *tmshandle = NULL;

	struct cap_param capp;
	struct cvt_param cvtp;
	struct enc_param encp;
	struct pac_param pacp;
	struct net_param netp;
	struct tms_param tmsp;

	int stage = 0b00000011;

	U32 vfmt = V4L2_PIX_FMT_YUYV;
	U32 ofmt = V4L2_PIX_FMT_YUV420;

	// set default values
	capp.dev_name = "/dev/video0";
	capp.width = 640;
	capp.height = 480;
	capp.pixfmt = vfmt;
	capp.rate = 15;

	cvtp.inwidth = 640;
	cvtp.inheight = 480;
	cvtp.inpixfmt = vfmt;
	cvtp.outwidth = 640;
	cvtp.outheight = 480;
	cvtp.outpixfmt = ofmt;

	encp.src_picwidth = 640;
	encp.src_picheight = 480;
	encp.enc_picwidth = 640;
	encp.enc_picheight = 480;
	encp.chroma_interleave = 0;
	encp.fps = 15;
	encp.gop = 12;
	encp.bitrate = 1000;

	pacp.max_pkt_len = 1400;
	pacp.ssrc = 1234;

	netp.serip = NULL;
	netp.serport = -1;
	netp.type = UDP;

	tmsp.startx = 10;
	tmsp.starty = 10;
	tmsp.video_width = 640;
	tmsp.factor = 0;

	char *outfile = NULL;
	// options
	int opt = 0;
	static const char *optString = "?vdi:o:a:p:w:h:r:f:t:g:s:c:";

	opt = getopt(argc, argv, optString);
	while (opt != -1)
	{
		int fmt;
		switch (opt)
		{
			case '?':
				display_usage();
				return 0;
			case 'v':
				display_version();
				return 0;
			case 'd':
				debug = 1;
				break;
			case 's':
				stage = atoi(optarg);
				break;
			case 'i':
				capp.dev_name = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'a':
				netp.serip = optarg;
				break;
			case 'p':
				netp.serport = atoi(optarg);
				break;
			case 'c':
				fmt = atoi(optarg);
				if (fmt == 1)
					capp.pixfmt = V4L2_PIX_FMT_YUV420;
				else
					capp.pixfmt = V4L2_PIX_FMT_YUYV;
				break;
			case 'w':
				capp.width = cvtp.inwidth = cvtp.outwidth = encp.src_picwidth =
						encp.enc_picwidth = tmsp.video_width = atoi(optarg);
				break;
			case 'h':
				capp.height = cvtp.inheight = cvtp.outheight =
						encp.src_picheight = encp.enc_picheight = atoi(optarg);
				break;
			case 'r':
				encp.bitrate = atoi(optarg);
				break;
			case 'f':
				capp.rate = encp.fps = atoi(optarg);
				break;
			case 't':
				encp.chroma_interleave = atoi(optarg);
				break;
			case 'g':
				encp.gop = atoi(optarg);
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

	// print version when start work
	display_version();

	caphandle = capture_open(capp);
	if (!caphandle)
	{
		printf("--- Open capture failed\n");
		return -1;
	}

	if ((stage & 0b00000001) != 0)
	{
		cvthandle = convert_open(cvtp);
		if (!cvthandle)
		{
			printf("--- Open convert failed\n");
			return -1;
		}
	}

	if ((stage & 0b00000010) != 0)
	{
		enchandle = encode_open(encp);
		if (!enchandle)
		{
			printf("--- Open encode failed\n");
			return -1;
		}
	}

	if ((stage & 0b00000100) != 0)
	{
		pachandle = pack_open(pacp);
		if (!pachandle)
		{
			printf("--- Open pack failed\n");
			return -1;
		}
	}

	if ((stage & 0b00001000) != 0)
	{
		if (netp.serip == NULL || netp.serport == -1)
		{
			printf(
					"--- Server ip and port must be specified when using network\n");
			return -1;
		}

		nethandle = net_open(netp);
		if (!nethandle)
		{
			printf("--- Open network failed\n");
			return -1;
		}
	}

	// timestamp try
	tmshandle = timestamp_open(tmsp);

	// start capture encode loop
	int ret;
	void *cap_buf, *cvt_buf, *hd_buf, *enc_buf, *pac_buf;
	int cap_len, cvt_len, hd_len, enc_len, pac_len;
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
		if (debug)		// print fps
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
				printf("\n*** FPS: %ld\n", fps_counter);

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
		if (debug)
			fputc('.', stdout);

		if ((stage & 0b00000001) == 0)    // no convert, capture only
		{
			if (outfd)
				fwrite(cap_buf, 1, cap_len, outfd);

			continue;
		}

		// convert
		if (capp.pixfmt == V4L2_PIX_FMT_YUV420)    // no need to convert
		{
			cvt_buf = cap_buf;
			cvt_len = cap_len;
		}
		else	// do convert: YUYV => YUV420
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
		}
		if (debug)
			fputc('-', stdout);

		// here add timestamp
		timestamp_draw(tmshandle, cvt_buf);

		if ((stage & 0b00000010) == 0)		// no encode
		{
			if (outfd)
				fwrite(cvt_buf, 1, cvt_len, outfd);

			continue;
		}

		// encode
		// fetch h264 headers first!
		while ((ret = encode_get_headers(enchandle, &hd_buf, &hd_len, &ptype))
				!= 0)
		{
			if (debug)
				fputc('S', stdout);

			if ((stage & 0b00000100) == 0)		// no pack
			{
				if (outfd)
					fwrite(hd_buf, 1, hd_len, outfd);

				continue;
			}

			// pack headers
			pack_put(pachandle, hd_buf, hd_len);
			while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
			{
				if (debug)
					fputc('#', stdout);

				if ((stage & 0b00001000) == 0)    // no network
				{
					if (outfd)
						fwrite(pac_buf, 1, pac_len, outfd);

					continue;
				}

				// network
				ret = net_send(nethandle, pac_buf, pac_len);
				if (ret != pac_len)
				{
					printf("send pack failed, size: %d, err: %s\n", pac_len,
							strerror(errno));
				}
				if (debug)
					fputc('>', stdout);
			}
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
		}

		if ((stage & 0b00000100) == 0)		// no pack
		{
			if (outfd)
				fwrite(enc_buf, 1, enc_len, outfd);

			continue;
		}

		// pack
		pack_put(pachandle, enc_buf, enc_len);
		while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
		{
			if (debug)
				fputc('#', stdout);

			if ((stage & 0b00001000) == 0)    // no network
			{
				if (outfd)
					fwrite(pac_buf, 1, pac_len, outfd);

				continue;
			}

			// network
			ret = net_send(nethandle, pac_buf, pac_len);
			if (ret != pac_len)
			{
				printf("!!! send pack failed, size: %d, err: %s\n", pac_len,
						strerror(errno));
			}
			if (debug)
				fputc('>', stdout);
		}
	}
	capture_stop(caphandle);

	if ((stage & 0b00001000) != 0)
		net_close(nethandle);
	if ((stage & 0b00000100) != 0)
		pack_close(pachandle);
	if ((stage & 0b00000010) != 0)
		encode_close(enchandle);
	if ((stage & 0b00000001) != 0)
		convert_close(cvthandle);
	capture_close(caphandle);

	timestamp_close(tmshandle);

	if (outfd)
		fclose(outfd);
	return 0;
}
