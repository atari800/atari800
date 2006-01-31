/*
 * VMU routines, taken mostly from dcgnuboy
 * (c) by Takayama Fumihiko <tekezo@catv296.ne.jp>
 */

#include <kos.h>
#include "vmu.h"

static const char *progname = "**ATARI800DC**";

/*  This rouine came from Marcus's website */
/*  http://mc.pp.se/dc/vms/fileheader.html */
static uint16 calc_crc(const uint8 *buf, int size)
{
	int i, c, n = 0;
	for (i = 0; i < size; i++) {
		n ^= (buf[i] << 8);
		for (c = 0; c < 8; c++)
			if (n & 0x8000)
				n = (n << 1) ^ 4129;
			else
				n = (n << 1);
	}
	return (n & 0xffff);
}


static void create_desc_short(char *dst, const char *src)
{
	int i;
	int src_len = strlen (src);

	for (i = 0; i < 16; ++i) {
		if (i < src_len)
			dst[i] = src[i];
		else
			dst[i] = ' ';
	}
}


static void create_desc_long(char *dst, const char *src)
{
	int i;
	int src_len = strlen (src);

	for (i = 0; i < 32; ++i) {
		if (i < src_len)
			dst[i] = src[i];
		else
			dst[i] = ' ';
	}
}


void ndc_vmu_do_crc (uint8 *buffer, int32 bytes)
{
	uint16 crc = calc_crc (buffer, bytes);
	struct file_hdr_vmu *hdr = (struct file_hdr_vmu *)buffer;

	hdr->crc = crc;
}


void ndc_vmu_create_vmu_header(uint8 *header, const char *desc_short,
			       const char *desc_long, uint32 filesize,
			       const uint8 *icon)
{
	struct file_hdr_vmu *hdr = (struct file_hdr_vmu *)header;

	memset(header, 0, 640);
	create_desc_short (hdr->desc_short, desc_short);
	create_desc_long (hdr->desc_long, desc_long);
	strcpy (hdr->app_id, progname);

	hdr->icon_cnt = 1;
	hdr->file_bytes = filesize;

	if (icon)
		memcpy(hdr->palette, icon, 32 + 512);
	else
		memset(hdr->palette, 0, 32 + 512);
}

