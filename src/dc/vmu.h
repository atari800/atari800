/*
 * VMU defines, taken mostly from dcgnuboy
 * (c) by Takayama Fumihiko <tekezo@catv296.ne.jp>
 */

#ifndef __VMU_H__
#define __VMU_H__

/* ROM menu header */
struct file_hdr_vmu {
	char	desc_short[16];
	char	desc_long[32];
	char	app_id[16];
	uint16	icon_cnt;
	uint16	animation_speed;
	uint16	eyecatch_type;
	uint16	crc;
	uint32	file_bytes;
	char	reserved[20];
	uint16	palette[16];
	uint8	icon_bitmap[384];
};

extern void ndc_vmu_do_crc (uint8 *buffer, int32 bytes);
extern void ndc_vmu_create_vmu_header(uint8 *header, const char *desc_short,
				      const char *desc_long, uint32 filesize,
				      const uint8 *icon);

#endif /* #ifndef __VMU_H__ */
