#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>

#include "prompts.h"

#define CONFIG_VERSION	"# Atari 800 Emulator configuration file ver.1.0"
#define MAX_CONFNAMES	(256*16)
#define MAX_CONF	256

jmp_buf jmpbuf;

void bus_err()
{
	longjmp(jmpbuf, 1);
}

int unaligned_long_ok()
{
#ifdef DJGPP
	return 1;
#else
	long l[2];

	if (setjmp(jmpbuf) == 0) {
		signal(SIGBUS, bus_err);
		*((long *) ((char *) l + 1)) = 1;
		signal(SIGBUS, SIG_DFL);
		return 1;
	}
	else {
		signal(SIGBUS, SIG_DFL);
		return 0;
	}
#endif
}

int build_in_test(char *t)
{
	if (!strcmp("UNALIGNED_LONG_OK", t)) {
		printf("Testing unaligned long accesses...");
		if (unaligned_long_ok()) {
			printf("OK\n");
			return (1);
		}
		else {
			printf("not OK\n");
			return (0);
		}
	}
	else if (!strcmp("ATARI800_BIG_ENDIAN", t)) {
		int i = 1;
		printf("Checking endianess... ");
		/* if first byte of i is 1, then it's little endian */
		if (*(char *) &i) {
			printf("little endian\n");
			return 0;
		}
		else {
			printf("big endian\n");
			return 1;
		}
	}
	else if (!strcmp("ATARI800_64_BIT", t))
		return sizeof(long) != 4;
	else
		return 0;
}

int makefile_defined(char *t)
{
	if (!strcmp("LINUX", t)) {
#ifdef linux
		return (1);
#endif
	}

	else if (!strcmp("SVGALIB", t)) {
#ifdef SVGALIB
		return (1);
#endif
	}

	else if (!strcmp("X11", t)) {
#ifdef X11
		return (1);
#endif
	}

	else if (!strcmp("ALLEGRO", t)) {
#ifdef AT_USE_ALLEGRO
		return (1);
#endif
	}

	return 0;
}

int main(void)
{
	FILE *fp, *fin;
	char config_filename[256];
	char *home;

	char buf[256];

	char conf_names[MAX_CONFNAMES];
	char *conf[MAX_CONF];
	char confyn[MAX_CONF];
	int pos_conf_names = 0;
	int pos_conf = 0;
	int i, j, yes, yes2, not;
	char *t;

	home = getenv("~");
	if (!home)
		home = getenv("HOME");
	if (!home)
		home = ".";

#ifndef DJGPP
	/* sprintf(config_filename, "%s/.atari800", home); */
	strcpy(config_filename, ".atari800");
#else
	/* sprintf(config_filename, "%s/atari800.djgpp", home); */
	strcpy(config_filename, "atari800.djgpp");
#endif

	fp = fopen(config_filename, "rt");
	if (fp) {
		printf("\nReading: %s\n\n", config_filename);

		fgets(buf, sizeof(buf), fp);
		RemoveLF(buf);

		if (!strcmp(CONFIG_VERSION, buf)) {
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				int i;
				if (buf[0] == '#')
					continue;

				RemoveLF(buf);
				/* remove the '=y' part */
				for (i = 0; i < strlen(buf); i++) {
					if (buf[i] == '=') {
						buf[i] = '\0';
						break;
					}
				}

				if (pos_conf >= MAX_CONF ||
					pos_conf_names + strlen(buf) + 1
					>= MAX_CONFNAMES) {
					printf("Out of memory\n");
					exit(1);
				}
				confyn[pos_conf] = 'C';
				conf[pos_conf++] = conf_names + pos_conf_names;
				strcpy(conf_names + pos_conf_names, buf);
				pos_conf_names += strlen(buf) + 1;
			}
		}
		else {
			printf("Cannot use this configuration file\n");
		}

		fclose(fp);
	}

	if ((fin = fopen("config.in", "rt")) == NULL)
		exit(1);
	if ((fp = fopen("config.h", "wt")) == NULL)
		exit(1);
	fprintf(fp, "/* This file is automaticaly generated. "
			"Do not edit!\n"
			" */\n");
	while (fgets(buf, sizeof(buf), fin) != NULL) {
		if (buf[0] != '?') {
			fputs(buf, fp);
			continue;
		}
		RemoveLF(buf);
		i = 1;
		not = 0;
		if (buf[i] == '!') {
			i++;
			not = 1;
		}
		yes = 1;
/* condition begin */
		if (buf[i] == '(') {
			i++;
			if (buf[i] == 0)
				exit(1);
			while (buf[i] != ')' && buf[i] != 0) {
				yes2 = 0;
				if (buf[i] == '!') {
					i++;
					yes2 = 1;
				}
				t = buf + i;
				while (buf[i] != 0 && buf[i] != ',' && buf[i] != ')')
					i++;
				if (buf[i] == 0)
					exit(1);
				if (buf[i] == ')')
					buf[i] = 0;
				else
					buf[i++] = 0;
				for (j = 0; j < pos_conf; j++) {
					if (!strcmp(t, conf[j])) {
						if (confyn[j] == 'C')
							confyn[j] = 'Y';
						if (confyn[j] == 'Y')
							yes2 = !yes2;
						else if (confyn[j] == 'y')
							yes2 = !yes2;
						break;
					}
				}

				if (j == pos_conf) { /* for() didn't break */
					if (makefile_defined(t))
						yes2 = !yes2;
				}

				if (!yes2)
					yes = 0;
			}
			i++;
		}
/* condition end */
		t = buf + i;
		while (buf[i] != 0 && buf[i] != ' ' && buf[i] != '\t')
			i++;
		if (buf[i] != 0) {
			buf[i++] = 0;
			while (buf[i] == ' ' || buf[i] == '\t')
				i++;
		}
		for (j = 0; j < pos_conf; j++) {
			if (!strcmp(t, conf[j])) {
				if (confyn[j] == 'C')
					confyn[j] = 'Y';
				break;
			}
		}
		if (j >= pos_conf) {
			j = pos_conf;
			if (pos_conf >= MAX_CONF ||
				pos_conf_names + strlen(buf) + 1
				>= MAX_CONFNAMES) {
				printf("Out of memory\n");
				exit(1);
			}
			confyn[pos_conf] = 'N';
			conf[pos_conf++] = conf_names + pos_conf_names;
			strcpy(conf_names + pos_conf_names, t);
			pos_conf_names += strlen(t) + 1;
		}
		if (not)
			confyn[j] = 'Y' + 'N' - confyn[j];
		if (yes) {
			if (buf[i] == 0) {
				if (!build_in_test(t))
					yes = 0;
			}
			else {
				strcpy(buf + i + strlen(buf + i), " [%c] ");
				YesNo(buf + i, confyn + j);
				if (confyn[j] != 'Y')
					yes = 0;
			}
		}
		if ((yes && !not) || (!yes && not)) {
			confyn[j] = 'Y';
			fprintf(fp, "#define %s\n", t);
		}
		else {
			confyn[j] = 'N';
			fprintf(fp, "/* #define %s */\n", t);
		}
		if (buf[i] == 0)
			confyn[j] = tolower(confyn[j]);
	}
	fclose(fp);

	fp = fopen(config_filename, "wt");
	if (fp) {
		printf("\nWriting: %s\n\n", config_filename);

		fprintf(fp, "%s\n", CONFIG_VERSION);
		for (j = 0; j < pos_conf; j++) {
			if (confyn[j] == 'Y')
				fprintf(fp, "%s=y\n", conf[j]);
			else
				fprintf(fp, "# %s is not set\n", conf[j]);
		}
		fclose(fp);
	}
	else {
		perror(config_filename);
		exit(1);
	}

	return 0;
}
