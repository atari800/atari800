/*****************************************************************************
 *                                                                           *
 *  Atari800  Atari 800XL, etc. emulator                                     *
 *  ----------------------------------------------------------------------   *
 *  POKEY Chip Emulator,                                                     *
 *  "POKEYBENCH" Test and benchmark program for developers, V1.1             *
 *  by Michael Borisov                                                       *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 *                                                                           *
 *                 License Information and Copyright Notice                  *
 *                 ========================================                  *
 *                                                                           *
 *  Pokeybench is Copyright(c) 2002 by Michael Borisov                       *
 *                                                                           *
 *  This program is free software; you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation; either version 2 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program; if not, write to the Free Software              *
 *  Foundation, Inc., 59 Temple Place - Suite 330,                           *
 *                Boston, MA 02111-1307, USA.                                *
 *                                                                           *
 *****************************************************************************/

#include "pokeysnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* How many seconds of sound to generate per each test trial */
/* and how many runs buffer is regenerated */
#define MZM_BUF_TIME 1
#define MZM_BUF_RUNS 500

#define TEST_SAMPLE_RATE 44100


/* How many test trials to run for statistics */
#define TEST_TRIALS 5

/* Wrapper for fgets, removes trailing whitespace */
char* fgetl(char* s, int len, FILE* fs)
{
    char* s2;
    int i;

    s2 = fgets(s,len,fs);
    if(s2 == NULL)
        return s2;
    for(i=strlen(s)-1; i>=0; i--)
        if(isspace(s[i]))
            s[i] = '\0';
    return s2;
}

int pktest(unsigned char *audf, unsigned char *audc, unsigned char audctl, const char* ofn)
{
    unsigned char* buf;
    double tim;
    double rate;
    double rasum;
    double rasum2;
    double varian;
    double stddev;
    int i,j;
    time_t start,finish;
    FILE* ft;

    buf = malloc(TEST_SAMPLE_RATE * MZM_BUF_TIME);
    if(buf == NULL)
    {
        printf("Out of memory\n");
        return 1;
    }

    Pokey_sound_init(1790000,TEST_SAMPLE_RATE,1);
    Update_pokey_sound(_AUDF1,audf[0],0,1);
    Update_pokey_sound(_AUDC1,audc[0],0,1);
    Update_pokey_sound(_AUDF2,audf[1],0,1);
    Update_pokey_sound(_AUDC2,audc[1],0,1);
    Update_pokey_sound(_AUDF3,audf[2],0,1);
    Update_pokey_sound(_AUDC3,audc[2],0,1);
    Update_pokey_sound(_AUDF4,audf[3],0,1);
    Update_pokey_sound(_AUDC4,audc[3],0,1);
    Update_pokey_sound(_AUDCTL,audctl,0,1);
    Update_pokey_sound(_STIMER,0,0,1);

    rasum = 0.0;
    rasum2 = 0.0;

    for(i=0; i<TEST_TRIALS; i++)
    {
        time(&start);
        for(j=0; j<MZM_BUF_RUNS; j++)
        {
            Pokey_process(buf,MZM_BUF_TIME * TEST_SAMPLE_RATE);
        }
        time(&finish);
        tim = difftime(finish,start);
        rate = 1.0/tim*MZM_BUF_TIME*TEST_SAMPLE_RATE*MZM_BUF_RUNS;

        printf("Trial %2d: %9d samples, %10.0f samples/sec, total %2.0f seconds\n",
            i+1,MZM_BUF_TIME * TEST_SAMPLE_RATE * MZM_BUF_RUNS, rate, tim);
        rasum += rate;
        rasum2 += rate*rate;
    }

    printf("\nAverage %10.0f samples/sec\n",rasum/TEST_TRIALS);

    varian = (rasum2 - rasum*rasum/TEST_TRIALS)/(TEST_TRIALS-1);
    stddev = sqrt(varian);

    printf("Standard deviation: %10.0f samples/sec\n",stddev);

    if(!(ft=fopen(ofn,"wb")))
    {
        perror(ofn);
        free(buf);
        return 2;
    }

    i = fwrite(buf,1,MZM_BUF_TIME*TEST_SAMPLE_RATE,ft);

    free(buf);

    if(i<MZM_BUF_TIME*TEST_SAMPLE_RATE)
    {
        perror(ofn);
        fclose(ft);
        return 2;
    }

    fclose(ft);

    return 0;
}

int main(int argc, char* argv[])
{
    char paramfn[256];
    char ofn[256];
    FILE* fs;
    unsigned char params[9];
    unsigned char audf[4];
    unsigned char audc[4];
    unsigned int tmp;
    int i;
    int ecode;

    printf("PokeyBench (c) 2002 by Michael Borisov\n\n");

    /* Get command-line parameters */
    if(argc<2)
    {
        printf("Enter parameter file name: ");
        if(fgetl(paramfn,256,stdin) == NULL)
        {
            printf("Bad input\n");
            return 1;
        }
    }
    else
    {
        strncpy(paramfn,argv[1],255);
        paramfn[255] = '\0';
    }

    if(argc<3)
    {
        printf("Enter output file name: ");
        if(fgetl(ofn,256,stdin) == NULL)
        {
            printf("Bad input\n");
            return 1;
        }
    }
    else
    {
        strncpy(ofn,argv[2],255);
        ofn[255] = '\0';
    }


    /* Read parameter file */
    if(!(fs = fopen(paramfn,"r")))
    {
        perror(paramfn);
        return 2;
    }
    
    for(i=0; i<9; i++)
    {
        ecode = fscanf(fs,"%u",&tmp);
        if(ecode == EOF)
        {
            if(ferror(fs))
            {
                perror(paramfn);
                fclose(fs);
                return 2;
            }
            else
            {
                printf("%s: Unexpected end of file\n", paramfn);
                fclose(fs);
                return 2;
            }
        }
        else if(ecode != 1)
        {
            printf("%s: Error in file format\n", paramfn);
            fclose(fs);
            return 2;
        }
        params[i] = (unsigned char)tmp;
    }
    fclose(fs);

    for(i=0; i<4; i++)
    {
        audf[i] = params[i*2];
        audc[i] = params[i*2+1];
    }
    
    return pktest(audf,audc,params[8],ofn);
}
