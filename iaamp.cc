//////////////////////////////////////////////////////////////////////
//
// Quick and dirty script that takes each sample from a sound file
// and raises it to a power. This was useful in speech/nonspeech
// and diarization to boost speech while not boosting noise as much.
// 0.75 was a good value.
//
//
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sndfile.h>

#define BUFSIZE (32000)

char* ProgName;

void usage();


int main(int argc, char** argv) {
  SNDFILE* insnd;
  SNDFILE* outsnd;
  SF_INFO ininfo;
  SF_INFO outinfo;
  float buf[BUFSIZE];
  int nread;
  int ii;
  float aval;
  float alpha;

  ProgName = argv[0];

  if (argc != 4) {
    usage();
  }

  alpha = 1.0;
  alpha = atof(argv[3]);
  
  if ((insnd = sf_open(argv[1], SFM_READ, &ininfo)) == NULL) {
    fprintf(stderr, "%s: couldn't open input sound '%s'\n",
	    ProgName, argv[1]);
    exit(EXIT_FAILURE);
  }
  if (ininfo.channels != 1) {
    fprintf(stderr, "%s: %s is a multi channel sound files. Not (yet) supported.\n",
	    ProgName, argv[1]);
    exit(1);
  }
  memcpy(&outinfo, &ininfo, sizeof(SF_INFO));
  if ((outsnd = sf_open(argv[2], SFM_WRITE, &outinfo)) == NULL) {
    fprintf(stderr, "%s: couldn't open output file '%s'\n",
	    ProgName, argv[2]);
    exit(EXIT_FAILURE);
  }

  while ((nread = sf_read_float(insnd, buf, BUFSIZE)) > 0) {
    for (ii = 0; ii < nread; ii++) {
      // Compute power on absolute value of signal
      // Preserve sign
      aval = fabs(buf[ii]);
      buf[ii] = pow(aval, alpha)*(signbit(buf[ii])?-1:1);
    }
    sf_write_float(outsnd, buf, nread);
  }
  
  sf_close(outsnd);
  sf_close(insnd);
  
  return(EXIT_SUCCESS);
}


void usage() {
  fprintf(stderr, "\nUsage: %s insnd outsnd exp\n\n", ProgName);
  fprintf(stderr, "\nApply an exponential power to every sample in an audio file.\n\n");
  exit(EXIT_FAILURE);
}

