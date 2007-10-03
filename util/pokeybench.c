/*****************************************************************************
 *                                                                           *
 *  Atari800  Atari 800XL, etc. emulator                                     *
 *  ----------------------------------------------------------------------   *
 *  POKEY Chip Emulator,                                                     *
 *  "POKEYBENCH" Test and benchmark program for developers, V1.3             *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,                           *
 *                Boston, MA 02110-1301, USA.                                *
 *                                                                           *
 *****************************************************************************/

#include "pokeysnd.h"
#include "mzpokeysnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* How many seconds of sound to generate per each test trial */
#define MZM_TRIAL_TIME 2

/* How many samples per each buffer run */
#define MZM_BUF_SAMPLES 100

/* How many test trials to run for statistics */
#define TEST_TRIALS 5

/* How many seconds of sound to save in the outfile */
#define MZM_SAVE_TIME 10


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

int pktest(unsigned char *audf, unsigned char *audc, unsigned char audctl,
           const char* ofn8, const char* ofn16,
           unsigned short samplerate)
{
    unsigned char* buf;
    short* buf16;
    double rate;
    double rasum;
    double rasum2;
    double varian;
    double stddev;
    unsigned long samremain, samproc;
    int i;
    time_t start,finish;
    FILE* ft;

    buf = malloc(MZM_BUF_SAMPLES);
    if(buf == NULL)
    {
        printf("Out of memory\n");
        return 1;
    }

    if(i=Pokey_sound_init(1790000,samplerate,1,0,1))
    {
        printf("Error initializing Pokey sound: %d\n",i);
        return 1;
    }

    Update_pokey_sound(_AUDF1,audf[0],0,1);
    Update_pokey_sound(_AUDC1,audc[0],0,1);
    Update_pokey_sound(_AUDF2,audf[1],0,1);
    Update_pokey_sound(_AUDC2,audc[1],0,1);
    Update_pokey_sound(_AUDF3,audf[2],0,1);
    Update_pokey_sound(_AUDC3,audc[2],0,1);
    Update_pokey_sound(_AUDF4,audf[3],0,1);
    Update_pokey_sound(_AUDC4,audc[3],0,1);
    Update_pokey_sound(_AUDCTL,audctl,0,1);
    Pokey_debugreset(0);
    

    rasum = 0.0;
    rasum2 = 0.0;

    for(i=0; i<TEST_TRIALS; i++)
    {
        rate = 0.0;
        /* Wait for a change in system time (seconds)
            to ensure start when a full second begins */
        time(&start);
        do
        {
            time(&finish);
        } while(difftime(finish,start) == 0.0);

        start = finish; /* Test start time */
        /* Generate until test time elapses */
        do
        {
            Pokey_process(buf,MZM_BUF_SAMPLES);
            rate += MZM_BUF_SAMPLES;
            time(&finish);
        } while(difftime(finish,start) < MZM_TRIAL_TIME);

        rate /= MZM_TRIAL_TIME;

        printf("Trial %2d:  %10.0f samples/sec\n",
            i+1, rate);
        fflush(stdout);
        rasum += rate;
        rasum2 += rate*rate;
    }

    printf("\nAverage %10.0f samples/sec\n",rasum/TEST_TRIALS);

    varian = (rasum2 - rasum*rasum/TEST_TRIALS)/(TEST_TRIALS-1);
    stddev = sqrt(varian);

    printf("Standard deviation: %10.0f samples/sec\n",stddev);

    printf("Gen/play ratio = %3.1f\n",rasum/TEST_TRIALS/samplerate);

    /* And now, write 8-bit output file */

    if(i=Pokey_sound_init(1790000,samplerate,1,0,1))
    {
        printf("Error initializing Pokey sound: %d\n",i);
        free(buf);
        return 1;
    }
    Update_pokey_sound(_AUDF1,audf[0],0,1);
    Update_pokey_sound(_AUDC1,audc[0],0,1);
    Update_pokey_sound(_AUDF2,audf[1],0,1);
    Update_pokey_sound(_AUDC2,audc[1],0,1);
    Update_pokey_sound(_AUDF3,audf[2],0,1);
    Update_pokey_sound(_AUDC3,audc[2],0,1);
    Update_pokey_sound(_AUDF4,audf[3],0,1);
    Update_pokey_sound(_AUDC4,audc[3],0,1);
    Update_pokey_sound(_AUDCTL,audctl,0,1);
    Pokey_debugreset(0);

    if(!(ft=fopen(ofn8,"wb")))
    {
        perror(ofn8);
        free(buf);
        return 2;
    }

    samremain = samplerate*MZM_SAVE_TIME;
    while(samremain>0)
    {
        if(samremain>=MZM_BUF_SAMPLES)
        {
            samproc = MZM_BUF_SAMPLES;
        }
        else
        {
            samproc = samremain;
        }
        Pokey_process(buf,(unsigned short)samproc);
        i = fwrite(buf,1,samproc,ft);
        if(i<samproc)
        {
            perror(ofn8);
            free(buf);
            fclose(ft);
            return 2;
        }
        samremain -= samproc;
    }

    free(buf);
    fclose(ft);

    /* Write 16-bit output file */

    if(i=Pokey_sound_init(1790000,samplerate,1,SND_BIT16,1))
    {
        printf("Error initializing Pokey sound: %d\n",i);
        return 1;
    }
    Update_pokey_sound(_AUDF1,audf[0],0,1);
    Update_pokey_sound(_AUDC1,audc[0],0,1);
    Update_pokey_sound(_AUDF2,audf[1],0,1);
    Update_pokey_sound(_AUDC2,audc[1],0,1);
    Update_pokey_sound(_AUDF3,audf[2],0,1);
    Update_pokey_sound(_AUDC3,audc[2],0,1);
    Update_pokey_sound(_AUDF4,audf[3],0,1);
    Update_pokey_sound(_AUDC4,audc[3],0,1);
    Update_pokey_sound(_AUDCTL,audctl,0,1);
    Pokey_debugreset(0);


    buf16 = malloc(2*MZM_BUF_SAMPLES);
    if(buf16 == NULL)
    {
        printf("Out of memory\n");
        return 1;
    }

    if(!(ft=fopen(ofn16,"wb")))
    {
        perror(ofn16);
        free(buf16);
        return 2;
    }

    samremain = samplerate*MZM_SAVE_TIME;
    while(samremain>0)
    {
        if(samremain>=MZM_BUF_SAMPLES)
        {
            samproc = MZM_BUF_SAMPLES;
        }
        else
        {
            samproc = samremain;
        }
        Pokey_process(buf16,(unsigned short)samproc);
        i = fwrite(buf16,2,samproc,ft);
        if(i<samproc)
        {
            perror(ofn16);
            free(buf16);
            fclose(ft);
            return 2;
        }
        samremain -= samproc;
    }

    free(buf16);
    fclose(ft);

    return 0;
}

int main(int argc, char* argv[])
{
    char paramfn[256];
    char ofn8[256];
    char ofn16[256];
    FILE* fs;
    unsigned int params[10];
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
        fflush(stdout);
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
        printf("Enter output file name (8-bit): ");
        fflush(stdout);
        if(fgetl(ofn8,256,stdin) == NULL)
        {
            printf("Bad input\n");
            return 1;
        }
    }
    else
    {
        strncpy(ofn8,argv[2],255);
        ofn8[255] = '\0';
    }

    if(argc<4)
    {
        printf("Enter output file name (16-bit): ");
        fflush(stdout);
        if(fgetl(ofn16,256,stdin) == NULL)
        {
            printf("Bad input\n");
            return 1;
        }
    }
    else
    {
        strncpy(ofn16,argv[3],255);
        ofn16[255] = '\0';
    }


    /* Read parameter file */
    if(!(fs = fopen(paramfn,"r")))
    {
        perror(paramfn);
        return 2;
    }
    
    for(i=0; i<10; i++)
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
        params[i] = tmp;
    }
    fclose(fs);

    for(i=0; i<4; i++)
    {
        audf[i] = params[i*2];
        audc[i] = params[i*2+1];
    }
    
    return pktest(audf,audc,(unsigned char)params[8],ofn8, ofn16, (unsigned short)params[9]);
}
