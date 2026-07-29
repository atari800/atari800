/*
 * acidtest.c - automated Acid800 test suite checker
 *
 * Copyright (C) 2026 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/* Runs the Acid800 test suite (boot test/acid800.atr with -acid800 <file>)
 * and compares each test's on-screen verdict against the expected results
 * in <file>.  If <file> does not exist, it is created from the current
 * results and can be committed as the new baseline.
 *
 * Every test is reported as one of:
 *   SUCCESS     passed and was expected to pass
 *   EXP FAIL    failed but that is the known state of the emulation
 *   SKIPPED     skipped, as expected
 *   UNEXP PASS  passed although a failure was expected (an improvement;
 *               update the baseline)
 *   FAIL        failed although it used to pass - a regression
 *   NEW         not present in the expected results
 *   MISSING     present in the expected results but never reported
 * The process exits 0 unless there are FAIL or MISSING results.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "acidtest.h"
#include "log.h"
#include "memory.h"

int ACIDTEST_enabled = FALSE;

#define MAX_TESTS 128
#define NAME_LEN 64
#define SCREEN_BYTES (24 * 40)
#define SCAN_INTERVAL 8		/* frames between screen scans */
#define TIMEOUT_FRAMES 60000	/* 20 emulated PAL minutes */

typedef struct {
	char name[NAME_LEN];
	char verdict;		/* 'P'ass, 'F'ail, 'S'kipped */
	int matched;
} test_result;

static char results_filename[FILENAME_MAX];
static test_result seen[MAX_TESTS];
static int n_seen;
static test_result expected[MAX_TESTS];
static int n_expected;
/* the suite's own closing counts, to also catch changes in tests whose
   individual results were missed by the screen scraper */
static int total_pass = -1, total_fail = -1, total_skip = -1;
static int exp_pass = -1, exp_fail = -1, exp_skip = -1;

void ACIDTEST_Init(const char *filename)
{
	strncpy(results_filename, filename, sizeof(results_filename) - 1);
	ACIDTEST_enabled = TRUE;
}

static const char *verdict_str(char v)
{
	return v == 'P' ? "PASS" : v == 'F' ? "FAIL" : "SKIP";
}

/* Read the text mode screen as ASCII; wrapped lines join seamlessly. */
static void read_screen(char *buf)
{
	int addr = MEMORY_dGetByte(0x58) + (MEMORY_dGetByte(0x59) << 8); /* SAVMSC */
	int i;
	for (i = 0; i < SCREEN_BYTES; i++) {
		UBYTE b = MEMORY_dGetByte((UWORD) (addr + i)) & 0x7f; /* strip inverse */
		b = (UBYTE) (b < 64 ? b + 32 : b < 96 ? b - 64 : b);  /* screen code -> ATASCII */
		buf[i] = (b < 32 || b > 126) ? ' ' : (char) b;
	}
	buf[SCREEN_BYTES] = '\0';
}

static void record(const char *name, char verdict)
{
	int i;
	for (i = 0; i < n_seen; i++)
		if (strcmp(seen[i].name, name) == 0)
			return;
	if (n_seen >= MAX_TESTS)
		return;
	strcpy(seen[n_seen].name, name);
	seen[n_seen].verdict = verdict;
	n_seen++;
}

/* Collect "Some test name...Pass/FAIL/Skipped" results; returns TRUE once
   the suite has printed its closing summary. */
static int scan_screen(void)
{
	char buf[SCREEN_BYTES + 1];
	int i;
	read_screen(buf);
	for (i = 0; i + 10 < SCREEN_BYTES; i++) {
		char v = 0;
		int start;
		int len;
		if (memcmp(buf + i, "...", 3) != 0)
			continue;
		if (memcmp(buf + i + 3, "Pass", 4) == 0)
			v = 'P';
		else if (memcmp(buf + i + 3, "FAIL", 4) == 0)
			v = 'F';
		else if (memcmp(buf + i + 3, "Skipped", 7) == 0)
			v = 'S';
		if (v == 0)
			continue;
		/* the test name reaches back to the preceding run of blanks */
		start = i;
		while (start > 1 && i - start < NAME_LEN
		 && !(buf[start - 1] == ' ' && buf[start - 2] == ' '))
			start--;
		while (buf[start] == ' ')
			start++;
		len = i - start;
		if (len > 0 && len < NAME_LEN && memchr(buf + start, ':', len) != NULL) {
			char name[NAME_LEN];
			memcpy(name, buf + start, len);
			name[len] = '\0';
			record(name, v);
		}
	}
	if (strstr(buf, "All tests complete.") == NULL)
		return FALSE;
	{
		const char *p = strstr(buf, "Passed:");
		if (p != NULL)
			sscanf(p, "Passed: %d Failed: %d Skipped: %d",
			       &total_pass, &total_fail, &total_skip);
	}
	return TRUE;
}

static int load_expected(void)
{
	char line[NAME_LEN + 16];
	FILE *f = fopen(results_filename, "r");
	if (f == NULL)
		return FALSE;
	while (fgets(line, sizeof(line), f) != NULL && n_expected < MAX_TESTS) {
		char *name = strchr(line, '\t');
		size_t len;
		if (strncmp(line, "TOTALS\t", 7) == 0) {
			sscanf(line + 7, "%d %d %d", &exp_pass, &exp_fail, &exp_skip);
			continue;
		}
		if (name == NULL || (line[0] != 'P' && line[0] != 'F' && line[0] != 'S'))
			continue;
		name++;
		len = strlen(name);
		while (len > 0 && (name[len - 1] == '\n' || name[len - 1] == '\r'))
			name[--len] = '\0';
		if (len == 0 || len >= NAME_LEN)
			continue;
		strcpy(expected[n_expected].name, name);
		expected[n_expected].verdict = line[0];
		n_expected++;
	}
	fclose(f);
	return TRUE;
}

static void write_baseline(void)
{
	int i;
	FILE *f = fopen(results_filename, "w");
	if (f == NULL) {
		Log_print("acid800: cannot write '%s'", results_filename);
		exit(2);
	}
	fprintf(f, "TOTALS\t%d %d %d\n", total_pass, total_fail, total_skip);
	for (i = 0; i < n_seen; i++)
		fprintf(f, "%s\t%s\n", verdict_str(seen[i].verdict), seen[i].name);
	fclose(f);
	Log_print("acid800: no expected results yet, wrote a baseline with %d results to '%s'",
	          n_seen, results_filename);
}

static int report(void)
{
	int i, j;
	int successes = 0, expfails = 0, skips = 0, unexpected = 0, fails = 0, missing = 0;
	for (i = 0; i < n_seen; i++) {
		test_result *e = NULL;
		for (j = 0; j < n_expected; j++)
			if (strcmp(expected[j].name, seen[i].name) == 0) {
				e = &expected[j];
				break;
			}
		if (e == NULL) {
			printf("NEW        %s (%s)\n", seen[i].name, verdict_str(seen[i].verdict));
			continue;
		}
		e->matched = TRUE;
		if (seen[i].verdict == e->verdict) {
			if (seen[i].verdict == 'P') {
				printf("SUCCESS    %s\n", seen[i].name);
				successes++;
			}
			else if (seen[i].verdict == 'F') {
				printf("EXP FAIL   %s\n", seen[i].name);
				expfails++;
			}
			else {
				printf("SKIPPED    %s\n", seen[i].name);
				skips++;
			}
		}
		else if (seen[i].verdict == 'P') {
			printf("UNEXP PASS %s (expected %s)\n", seen[i].name, verdict_str(e->verdict));
			unexpected++;
		}
		else {
			printf("FAIL       %s (expected %s, got %s)\n", seen[i].name,
			       verdict_str(e->verdict), verdict_str(seen[i].verdict));
			fails++;
		}
	}
	for (j = 0; j < n_expected; j++)
		if (!expected[j].matched) {
			printf("MISSING    %s\n", expected[j].name);
			missing++;
		}
	if (exp_pass >= 0
	 && (total_pass != exp_pass || total_fail != exp_fail || total_skip != exp_skip)) {
		if (total_pass >= exp_pass && total_fail <= exp_fail)
			printf("UNEXP PASS suite totals improved: passed/failed/skipped %d/%d/%d, expected %d/%d/%d\n",
			       total_pass, total_fail, total_skip, exp_pass, exp_fail, exp_skip);
		else {
			printf("FAIL       suite totals changed: passed/failed/skipped %d/%d/%d, expected %d/%d/%d\n",
			       total_pass, total_fail, total_skip, exp_pass, exp_fail, exp_skip);
			fails++;
		}
	}
	printf("acid800: %d success, %d expected failures, %d skipped, %d unexpected passes, %d FAILED, %d missing\n",
	       successes, expfails, skips, unexpected, fails, missing);
	return (fails == 0 && missing == 0) ? 0 : 1;
}

void ACIDTEST_Frame(void)
{
	if (Atari800_nframes > TIMEOUT_FRAMES) {
		printf("acid800: timeout, the suite did not complete (%d results seen)\n", n_seen);
		exit(2);
	}
	if (Atari800_nframes % SCAN_INTERVAL != 0)
		return;
	if (!scan_screen())
		return;
	if (load_expected())
		exit(report());
	write_baseline();
	exit(0);
}
