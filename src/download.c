#include "download.h"

#ifdef HAVE_DOWNLOAD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <zlib.h>
#include "log.h"
#include "util.h"


#ifdef HAVE_WINDOWS_H
#include <direct.h>
#endif

struct MemoryBuffer {
	unsigned char *data;
	size_t size;
};

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;
	unsigned char *ptr = realloc(mem->data, mem->size + realsize);
	if (ptr == NULL)
		return 0;
	mem->data = ptr;
	memcpy(mem->data + mem->size, contents, realsize);
	mem->size += realsize;
	return realsize;
}

static int ReadLE16(const unsigned char *p)
{
	return p[0] | (p[1] << 8);
}

static int ReadLE32(const unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static int CreateDir(const char *path)
{
#ifdef HAVE_WINDOWS_H
	return mkdir(path);
#else
	return mkdir(path, 0777);
#endif
}

static int HasAnyExt(const char *name, const char *exts[])
{
	const char *dot = strrchr(name, '.');
	if (dot == NULL) return 0;
	for (int i = 0; exts[i] != NULL; i++) {
		if (Util_stricmp(dot, exts[i]) == 0)
			return 1;
	}
	return 0;
}

static void MkdirParent(char *path)
{
	char *p;
	for (p = path + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = '\0';
			CreateDir(path);
			*p = '/';
		}
	}
}

const char *Download_And_Extract(const char *url, const char *exts[], const char *dest_dir)
{
	CURL *curl;
	CURLcode res;
	struct MemoryBuffer buf = {NULL, 0};
	size_t offset;
	static char first_file[FILENAME_MAX] = "";

	curl = curl_easy_init();
	if (curl == NULL)
		return NULL;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		Log_print("Download failed: %s", curl_easy_strerror(res));
		free(buf.data);
		return NULL;
	}

	/* parse zip local file headers */
	offset = 0;
	{
		int cd_count = 0;
		struct {
			unsigned int local_offset;
			unsigned int comp_size;
			unsigned int uncomp_size;
		} cd_entry[256];
		/* Parse central directory for correct sizes (some zips use data descriptors with 0 in local header) */
		if (buf.size >= 22) {
			size_t search_start = (buf.size > 65557) ? buf.size - 65557 : 0;
			size_t search_end = buf.size - 22;
			for (size_t i = search_start; i <= search_end; i++) {
				if (buf.data[i] == 0x50 && buf.data[i+1] == 0x4B
				    && buf.data[i+2] == 0x05 && buf.data[i+3] == 0x06) {
					unsigned int cd_offset = ReadLE32(buf.data + i + 16);
					unsigned int cd_total = ReadLE16(buf.data + i + 10);
					size_t cd_pos = cd_offset;
					for (unsigned int j = 0; j < cd_total && cd_pos + 46 <= buf.size; j++) {
						if (buf.data[cd_pos] != 0x50 || buf.data[cd_pos+1] != 0x4B
						    || buf.data[cd_pos+2] != 0x01 || buf.data[cd_pos+3] != 0x02)
							break;
						if (cd_count < 256) {
							cd_entry[cd_count].local_offset = ReadLE32(buf.data + cd_pos + 42);
							cd_entry[cd_count].comp_size = ReadLE32(buf.data + cd_pos + 20);
							cd_entry[cd_count].uncomp_size = ReadLE32(buf.data + cd_pos + 24);
							cd_count++;
						}
						unsigned int fn_len = ReadLE16(buf.data + cd_pos + 28);
						unsigned int ef_len = ReadLE16(buf.data + cd_pos + 30);
						unsigned int cm_len = ReadLE16(buf.data + cd_pos + 32);
						cd_pos += 46 + fn_len + ef_len + cm_len;
					}
					break;
				}
			}
		}

		int extracted = 0;
		while (offset + 30 <= buf.size) {
			unsigned short flags, compression, name_len, extra_len;
			unsigned int comp_size, uncomp_size;
			size_t header_size;
			char *name;
			unsigned char *out_data;
			size_t out_size;

			if (buf.data[offset] != 'P' || buf.data[offset + 1] != 'K'
			    || buf.data[offset + 2] != 0x03 || buf.data[offset + 3] != 0x04)
				break;

			flags = ReadLE16(buf.data + offset + 6);
			compression = ReadLE16(buf.data + offset + 8);
			name_len = ReadLE16(buf.data + offset + 26);
			extra_len = ReadLE16(buf.data + offset + 28);
			comp_size = ReadLE32(buf.data + offset + 18);
			uncomp_size = ReadLE32(buf.data + offset + 22);
			if ((flags & 0x08) && comp_size == 0) {
				for (int k = 0; k < cd_count; k++) {
					if (cd_entry[k].local_offset == (unsigned int)offset) {
						comp_size = cd_entry[k].comp_size;
						uncomp_size = cd_entry[k].uncomp_size;
						break;
					}
				}
			}

			header_size = 30 + name_len + extra_len;
			if (offset + header_size > buf.size)
				break;

			name = malloc(name_len + 1);
			if (name == NULL)
				break;
			memcpy(name, buf.data + offset + 30, name_len);
			name[name_len] = '\0';

			offset += header_size;

			if (comp_size > 0 || uncomp_size > 0) {
				out_data = malloc(uncomp_size > 0 ? uncomp_size : 1);
				if (out_data == NULL) {
					free(name);
					break;
				}
				out_size = 0;

				if (compression == 0) {
					memcpy(out_data, buf.data + offset, comp_size);
					out_size = comp_size;
				}
				else if (compression == 8) {
					z_stream strm;
					int ret;

					memset(&strm, 0, sizeof(strm));
					inflateInit2(&strm, -MAX_WBITS);
					strm.next_in = buf.data + offset;
					strm.avail_in = comp_size;
					strm.next_out = out_data;
					strm.avail_out = uncomp_size;
					ret = inflate(&strm, Z_FINISH);
					if (ret == Z_STREAM_END)
						out_size = strm.total_out;
					inflateEnd(&strm);
				}

				if (out_size > 0
				    && name[name_len - 1] != '/'
				    && strstr(name, "__MACOSX") == NULL
				    && HasAnyExt(name, exts)) {
					char outpath[FILENAME_MAX];
					FILE *f;

					/* Strip zip directory path so files land flat in dest_dir. */
					const char *basename = strrchr(name, '/');
					if (basename != NULL)
						basename++;
					else
						basename = name;

					snprintf(outpath, sizeof(outpath), "%s/%s", dest_dir, basename);
					MkdirParent(outpath);
					f = fopen(outpath, "wb");
					if (f != NULL) {
						fwrite(out_data, 1, out_size, f);
						fclose(f);
						if (extracted == 0)
							strncpy(first_file, basename, sizeof(first_file)-1);
						extracted++;
					}
				}

				free(out_data);
			}

			offset += comp_size;
			free(name);
		}

		free(buf.data);
		if (extracted == 0) {
			Log_print("No matching files found in archive");
			return NULL;
		}
		return first_file;
	}
}

#endif /* HAVE_DOWNLOAD */
