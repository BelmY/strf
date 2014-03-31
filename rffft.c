#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <fftw3.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>

int main(int argc,char *argv[])
{
  int i,j,k,l,m,nchan,nint=1,arg=0,nbytes,nsub=60,flag;
  fftwf_complex *c,*d;
  fftwf_plan fft;
  FILE *infile,*outfile;
  char infname[128],outfname[128],path[64]="/data/record",prefix[32]="";
  float *fbuf;
  int16_t *ibuf;
  float *z,length,fchan=100.0,tint=1.0;
  double freq,samp_rate;
  struct timeval start,end;
  char tbuf[30],nfd[32],header[256]="";

  // Read arguments
  while ((arg=getopt(argc,argv,"i:f:s:c:t:p:n:"))!=-1) {
    switch(arg) {

    case 'i':
      strcpy(infname,optarg);
      break;

    case 'p':
      strcpy(path,optarg);
      break;

    case 'f':
      freq=(double) atof(optarg);
      break;

    case 's':
      samp_rate=(double) atof(optarg);
      break;
      
    case 'c':
      fchan=atof(optarg);
      break;

    case 'n':
      nsub=atoi(optarg);
      break;

    case 't':
      tint=atof(optarg);
      break;

    default:
      return 0;
    }
  }

  // Number of channels
  nchan=(int) (samp_rate/fchan);

  // Number of integrations
  nint=(int) (tint*(float) samp_rate/(float) nchan);

  // Dump statistics
  printf("Filename: %s\n",infname);
  printf("Bandwidth: %f MHz\n",samp_rate*1e-6);
  printf("Sampling time: %f us\n",1e6/samp_rate);
  printf("Number of channels: %d\n",nchan);
  printf("Channel size: %f Hz\n",samp_rate/(float) nchan);
  printf("Integration time: %f s\n",tint);
  printf("Number of averaged spectra: %d\n",nint);
  printf("Number of subints per file: %d\n",nsub);

  // Allocate
  c=fftwf_malloc(sizeof(fftwf_complex)*nchan);
  d=fftwf_malloc(sizeof(fftwf_complex)*nchan);
  fbuf=(float *) malloc(sizeof(float)*2*nchan);
  ibuf=(int16_t *) malloc(sizeof(int16_t)*2*nchan);
  z=(float *) malloc(sizeof(float)*nchan);

  // Plan
  fft=fftwf_plan_dft_1d(nchan,c,d,FFTW_FORWARD,FFTW_ESTIMATE);

  // Create prefix
  gettimeofday(&start,0);
  strftime(prefix,30,"%Y-%m-%dT%T",gmtime(&start.tv_sec));

  // Open file
  infile=fopen(infname,"r");

  // Forever loop
  for (m=0;;m++) {
    // File name
    sprintf(outfname,"%s/%s_%06d.bin",path,prefix,m);
    outfile=fopen(outfname,"w");

    // Loop over subints to dump
    for (k=0;k<nsub;k++) {
      // Initialize
      for (i=0;i<nchan;i++) 
	z[i]=0.0;
      
      // Log start time
      gettimeofday(&start,0);
      
      // Integrate
      for (j=0;j<nint;j++) {
	//	nbytes=fread(fbuf,sizeof(float),2*nchan,infile);
	nbytes=fread(ibuf,sizeof(int16_t),2*nchan,infile);
	if (nbytes==0)
	  break;
	
	// Unpack
	for (i=0;i<nchan;i++) {
	  c[i][0]=(float) ibuf[2*i]/32768.0;
	  c[i][1]=(float) ibuf[2*i+1]/32768.0;
	}
	
	// Execute
	fftwf_execute(fft);
	
	// Add
	for (i=0;i<nchan;i++) {
	  if (i<nchan/2)
	    l=i+nchan/2;
	  else
	    l=i-nchan/2;
	  
	  z[l]+=d[i][0]*d[i][0]+d[i][1]*d[i][1];
	}
      }

      // Log end time
      gettimeofday(&end,0);
      
      // Scale
      for (i=0;i<nchan;i++) 
	z[i]/=(float) nchan;
      
      // Time stats
      length=(end.tv_sec-start.tv_sec)+(end.tv_usec-start.tv_usec)*1e-6;
      
      // Format start time
      strftime(tbuf,30,"%Y-%m-%dT%T",gmtime(&start.tv_sec));
      sprintf(nfd,"%s.%03ld",tbuf,start.tv_usec/1000);

      // Header
      sprintf(header,"HEADER\nUTC_START    %s\nFREQ         %lf Hz\nBW           %lf Hz\nLENGTH       %f s\nNCHAN        %d\nNSUB         %d\nEND\n",nfd,freq,samp_rate,length,nchan,nsub);

      printf("%s %s %f %d\n",outfname,nfd,length,j);
      
      // Dump file
      fwrite(header,sizeof(char),256,outfile);
      fwrite(z,sizeof(float),nchan,outfile);
      
      // Break;
      if (nbytes==0)
	break;
    }

    // Break;
    if (nbytes==0)
      break;
    
    // Close file
    fclose(outfile);
  }
  fclose(infile);

  // Destroy plan
  fftwf_destroy_plan(fft);

  // Deallocate
  free(fbuf);
  free(ibuf);
  fftwf_free(c);
  fftwf_free(d);
  free(z);

  return 0;
}

