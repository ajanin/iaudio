//////////////////////////////////////////////////////////////////////
//
// File: iadiff.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Print out the difference between two audio files.
//
//
// 03/10/06  Adam Janin
//           Initial version.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sndfile.h>

//////////////////////////////////////////////////////////////////////
//
// Globals
//

char* ProgName;
float Threshold = 0.02;
int Silent = 0;
int GotDiff = 0;	// Becomes 1 if the files are different.

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();
static SNDFILE* open_sound(const char* fname, SF_INFO* info);
static void iadiff(SNDFILE* s1, SF_INFO* info1, SNDFILE* s2, SF_INFO* info2);
static float my_atof(const char* in);

//////////////////////////////////////////////////////////////////////
//
// Usage
//

void usage() {
  fprintf(stderr, "\nUsage: %s [-s] [-t threshold] sndfile1 sndfile2\n\n", ProgName);
  fprintf(stderr, "  Print out each frame for which sndfile1 and sndfile2 differ by more\n");
  fprintf(stderr, "  than threshold (defaults to 0.02).\n\n");
  fprintf(stderr, "  Note that values are printed as floating point numbers.\n\n");
  fprintf(stderr, "  The program uses libsndfile for I/O, so it supports all formats\n");
  fprintf(stderr, "  that libsndfile supports.\n\n");
  fprintf(stderr, "  If -s is given, do not print the differences. Instead, return 0\n");
  fprintf(stderr, "  if the files are within the threshold and 1 otherwise.\n\n");
  exit(-1);
} // usage()


//////////////////////////////////////////////////////////////////////
//
// Main
//

int main(int argc, char** argv) {
  SNDFILE *s1, *s2;
  SF_INFO info1, info2;
  int c;
  extern char *optarg;
  extern int optind;

  ProgName = argv[0];

  while ((c = getopt(argc, argv, "st:")) != EOF) {
    switch (c) {
    case 't':
      Threshold = my_atof(optarg);
      break;
    case 's':
      Silent = 1;
      break;
    }
  }
  
  if (argc != optind+2) {
    usage();
  }

  s1 = open_sound(argv[optind], &info1);
  s2 = open_sound(argv[optind+1], &info2);

  iadiff(s1, &info1, s2, &info2);
    
  sf_close(s1);
  sf_close(s2);
  
  return GotDiff;
} // main()

static SNDFILE* open_sound(const char* fname, SF_INFO* info) {
  SNDFILE* in;
  
  if ((in = sf_open(fname, SFM_READ, info)) == NULL) {
    fprintf(stderr, "%s: couldn't open '%s' as input sound file\n",
	    ProgName, fname);
    usage();
  }
  
  if (info->channels != 1) {
    fprintf(stderr, "%s: %s is a multi channel sound files. Not (yet) supported.\n",
	    ProgName, fname);
    exit(-1);
  }
  return in;
} // open_sound()


static void iadiff(SNDFILE* s1, SF_INFO* info1, SNDFILE* s2, SF_INFO* info2) {
  const long blocksize = 4096;
  int nread1, nread2, nread, i, pos;
  float diff;
  float buf1[blocksize];
  float buf2[blocksize];

  pos = 0;
  do {
    nread1 = sf_read_float(s1, buf1, blocksize);
    nread2 = sf_read_float(s2, buf2, blocksize);
    if (nread1 != nread2) {
      fprintf(stderr, "%s: Warning: file sizes appear to be different.\n", ProgName);
      GotDiff = 1;
    }
    nread = (nread1 < nread2) ? nread1 : nread2;
    for (i = 0; i < nread; i++) {
      diff = buf1[i] - buf2[i];
      if (diff <= -Threshold || diff >= Threshold) {
	GotDiff = 1;
	if (Silent) {
	  return;
	}
	printf("%6d  %6.3f  %6.3f   %6.3f\n", pos+i, buf1[i], buf2[i], diff);
      }
    }
    pos += nread;
  } while (nread1 == blocksize && nread2 == blocksize);
} // iadiff()

  
static float my_atof(const char* in) {
  float out;
  if (sscanf(in, "%f", &out) != 1) {
    fprintf(stderr, "Bad float value %s\n", in);
    usage();
  }
  return out;
}
