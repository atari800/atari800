#include <stdio.h>
#define SIZE 10
/*bdata.c: convert binary file to Atari basic data statements*/
int main(int argc,char *argv[]){
	int buf[SIZE];
	int i;
	int c;
	int num;
	int linenum=30000;
	int end=0;
	FILE *fpin,*fpout;
	if(argc!=3){
		printf("usage: %s infile outfile\n",argv[0]);
	}
	fpin=fopen(argv[1],"rb");
	fpout=fopen(argv[2],"wb");
	/*fopen(argv[2],"wb");*/
		
	while(!end){
		num=0;
		for(i=0;i<SIZE;i++){
			c=fgetc(fpin);
			if(c==EOF){
				end=1;
				buf[num]=-1;
				num++;
				break;
			}
			buf[num]=c;
			num++;
			
		}
		fprintf(fpout,"%d DATA ",linenum);
		for(i=0;i<num;i++){
			if(i!=0){
				fprintf(fpout,", ");
			}
			fprintf(fpout,"%d",buf[i]);
		}
		fprintf(fpout,"%c",155);
		linenum+=10;
	}

	
	return 0;
}
