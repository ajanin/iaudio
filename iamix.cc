/////////////////////////////////////////////////////////////////////
//
// File: iamix.cc
// Author: Adam Janin
//
// Copyright 2016 International Computer Science Institute
// See the file LICENSE for licensing terms.

// Originally from jsndmix.cc (02/21/00), which used internal ICSI
// libraries for audio and statistics handling, and was written in C++.
//
//
// Given a set of audio files in many different formats, produce an
// output audio file that is the mix of the input files.
//
// You can either provide a gain for each input audio file, or the
// program can estimate the gain required to equalize the volume.
// The autogain computation is quite primitive. See comments below.
//
// See usage() for command line arguments.
//


//
// The autogain is computed by normalizing the input signals by their
// standard deviation. This is computed by taking random samples of
// size RandomSampleSize seconds until RandomSampleTime seconds have
// been collected, and computing the stddev over that size. Note that
// this requires lots of seeks, and will be very inefficient if the
// stream is compressed or if RandomSampleTime is a large fraction of
// the file size. You can specify a maximum gain. This helps prevent
// very quiet channels from being boosted too much. You can also apply
// a gain setting to the resulting output audio. Using a value less
// than 1.0 helps prevent "clipping" when the signals are close to
// saturation.
// 

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <sndfile.h>

#include "sndstats.h"

//////////////////////////////////////////////////////////////////////
//
// Defines
//

#define MEMCHECK(x)                                                \
if (!x) {                                                          \
  fprintf(stderr, "Out of memory in %s line %d\n",                 \
          __FILE__, __LINE__);                                     \
  exit(1);                                                         \
}


//////////////////////////////////////////////////////////////////////
//
// Globals
//

char* ProgName;
int Verbose = 0;

// See comments above
int AutoGain = 0;
float RandomSampleTime = 300.0;	// 5 minutes
float RandomSampleSize = 2.0;	// 2 seconds

// Maximum gain to apply to any channel. <= 0 implies no max.
float MaxGain = -1.0;

// Buffer size (to hold one block of audio data while processing)
int BlockSize = 8192;

// Gain to apply to summed signal (after autogain or individual
// gains). Use a value less than 1.0 if there's clipping in the
// resulting file.
float Gain = 1.0;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();

void mix(int nfiles, SNDFILE** sounds, float* scales, SNDFILE* out);
void auto_gain(int nfiles, SNDFILE** sounds, SF_INFO* sfinfo, float* scales);
float std_snd(SNDFILE* in, SF_INFO* sfinfo);
float my_atof(char*);
int   my_atoi(char*);


//////////////////////////////////////////////////////////////////////
//
// Print command line usage and exit.
//

void usage() {
  fprintf(stderr, "\nUsage: %s -v -m maxgain -o outfile -g gain in1 sc1 in2 sc2 ...  OR\n",
	  ProgName);
  fprintf(stderr, "       %s -a -v -m maxgain -o outfile -g gain in1 in2 ...\n",
	  ProgName);
  fprintf(stderr, "where\n");
  fprintf(stderr, " -o outfile  Output file [stdout]\n");
  fprintf(stderr, " -g gain     Gain to apply to output file [1.0]\n");
  fprintf(stderr, " -m maxgain  Maximum gain to apply to any input file [-1.0]\n");
  fprintf(stderr, " -a          Compute gain (scales) automatically\n");
  fprintf(stderr, " -v          Verbose\n");
  fprintf(stderr, " inN         Input file\n");
  fprintf(stderr, " scN         Scale for input file (if -a isn't given)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "    If maxgain is less than 0.0, then there is no limit to the gain.\n");
  fprintf(stderr, "\n The following arguments are also allowed, but seldom needed:\n\n");
  fprintf(stderr, " -s size     Time in seconds of a single sample used to compute autogain [%f]\n", RandomSampleSize);
  fprintf(stderr, " -t time     Total amount of time in seconds used to compute autogain [%f]\n", RandomSampleTime);
  fprintf(stderr, " -b size     Number of samples to read at a time [%d]\n",
	  BlockSize);
  fprintf(stderr, "\n");
  exit(1);
}  // usage()


//////////////////////////////////////////////////////////////////////
//
// Main
//

int main(int argc, char** argv) {
  extern char *optarg;
  extern int optind;

  int c, i, nargs, nfiles;
  char *fname;

  float *scales;		// How much to scale the inputs
  SNDFILE* *sounds;
  SF_INFO *sfinfos;		// Information about input audio files
  char* outfn = NULL;
  SNDFILE* out;
  
  ProgName = argv[0];

  while ((c = getopt(argc, argv, "ab:g:m:o:s:t:v")) != EOF) {
    switch (c) {
    case 'a':
      AutoGain = 1;
      break;
    case 'b':
      BlockSize = my_atof(optarg);
      break;
    case 'g':
      Gain = my_atof(optarg);
      break;
    case 'm':
      MaxGain = my_atof(optarg);
      break;
    case 'o':
      outfn = strdup(optarg);
      break;
    case 's':
      RandomSampleSize = my_atof(optarg);
      break;
    case 't':
      RandomSampleTime = my_atof(optarg);
      break;
    case 'v':
      Verbose = 1;
      break;
    }
  }

  if (!outfn) {
    outfn = strdup("-"); // if not provided, stdout.
  }

  nargs = argc - optind;

  if (nargs < 1) {
    usage();
  }

  // If autogain is NOT set, then the arguments must be file1 gain1
  // file2 gain2 ..., and therefore there must be an even number of
  // arguments. 

  if (AutoGain == 0) {
    if (nargs % 2 != 0) {
      usage();
    }

    nfiles = nargs / 2;
  } else if (AutoGain == 1) {
    nfiles = nargs;
  } else {
    fprintf(stderr, "This should never happen!\n");
    exit(-1);
  }

  sounds = new SNDFILE*[nfiles];
  scales = new float[nfiles];
  sfinfos = new SF_INFO[nfiles];
  
  for (i = 0; i < nfiles; i++) {
    if (AutoGain == 0) {
      fname = argv[optind + 2*i];
    } else {
      fname = argv[optind+i];
    }
    
    sounds[i] = sf_open(fname, SFM_READ, &(sfinfos[i]));
    if (sounds[i] == NULL) {
      fprintf(stderr, "%s: couldn't open '%s' as input sound file: %s\n",
	      ProgName, fname, sf_strerror(NULL));
      usage();
    }

    if (sfinfos[i].channels != 1) {
      fprintf(stderr, "%s: Currently only one channel per file is supported.\n",
	      ProgName);
      exit(1);
    }
  }

  // Now either compute or extract the scale factors

  if (AutoGain == 0) {
    for (i = 0; i < nfiles; i++) {
      scales[i] = my_atof(argv[optind + 2*i+1]);
    }
  } else if (AutoGain == 1) {
    auto_gain(nfiles, sounds, sfinfos, scales);
  }

  // Use sfinfos[0] so that the output will be the same format as the
  // FIRST input.

  out = sf_open(outfn, SFM_WRITE, &(sfinfos[0]));
  
  if (!out) {
    fprintf(stderr, "%s: Couldn't open output file %s: %s\n",
	    ProgName, outfn, sf_strerror(NULL));
    exit(1);
  }
  
  for (i = 0; i < nfiles; i++) {

    // If MaxGain is set, clip the gains to MaxGain
    if (MaxGain > 0.0 && scales[i] > MaxGain) {
      scales[i] = MaxGain;
    }

    // Apply output gain (applying it here is the same as applying it
    // to the output file)
    scales[i] *= Gain;

    if (Verbose) {
      fprintf(stderr, "scale[%d] = %f\n", i, scales[i]);
    }
  }

  // Do the work

  mix(nfiles, sounds, scales, out);

  // Clean up and exit

  for (i = 0; i < nfiles; i++) {
    sf_close(sounds[i]);
  }
  sf_close(out);
  delete [] sounds;
  delete [] scales;
  delete [] sfinfos;
  free(outfn);
    
  return 0;
}  // main()

//////////////////////////////////////////////////////////////////////
//
// Do the work.
//
// If the files do not have the same sizes, they will be left
// justified, and the output will be the size of the longest input.
//

void mix(int nfiles, SNDFILE** sounds, float* scales, SNDFILE* out) {
  int i, j;
  long nread;
  long max_nread;		//  Biggest block read in any input
  float* buf;
  float* outbuf;
  int done = 0;
  int gotone;			// True if you got ANY data from an input

  buf = new float[BlockSize];
  outbuf = new float[BlockSize];
  
  if (Verbose) {
    fprintf(stderr, "Starting mix...\n");
  }

  while (!done) {
    for (j = 0; j < BlockSize; j++) {
      outbuf[j] = 0.0;
    }
    gotone = 0;
    max_nread = 0;
    for (i = 0; i < nfiles; i++) {
      nread = sf_read_float(sounds[i], buf, BlockSize);
      if (nread > 0) {
	gotone = 1;
	for (j = 0; j < nread; j++) {
	  outbuf[j] += buf[j] * scales[i];
	}
	if (nread > max_nread) max_nread = nread;
      }
    }
    if (gotone) {
      sf_write_float(out, outbuf, max_nread);
    } else {
      done = 1;
    }
  }
  delete [] buf;
  delete [] outbuf;
  
  if (Verbose) {
    fprintf(stderr, "Done.\n");
  }
}  // mix()

//////////////////////////////////////////////////////////////////////
//
// Given the set of sound files, compute all the scaling factors so
// that the loudest sound doesn't change, and all the others are
// equalized.
//
// Note: The sounds are all rewound to the start as a side effect.
//

void auto_gain(int nfiles, SNDFILE** sounds, SF_INFO* sfinfos, float* scales) {
  int i;
  float minscale;

  if (Verbose) {
    fprintf(stderr, "Computing auto-gain...\n");
  }

  scales[0] = 1.0 / std_snd(sounds[0], &(sfinfos[0]));
  minscale = scales[0];

  for (i = 1; i < nfiles; i++) {
    scales[i] = 1.0 / std_snd(sounds[i], &(sfinfos[i]));
    if (minscale > scales[i]) minscale = scales[i];
  }
  for (i = 0; i < nfiles; i++) {
    scales[i] /= minscale;
    sf_seek(sounds[i], 0, SEEK_SET);
  }
}  // auto_gain()

//////////////////////////////////////////////////////////////////////
//
// Compute the standard deviation of the given sound.
//

float std_snd(SNDFILE* in, SF_INFO* sfinfo) {
  jstat stat;  
  
  if (sfinfo->frames < RandomSampleTime * sfinfo->samplerate) {
    (void) sndstat(in, sfinfo, "iamix audio file", 0.0, -1.0, &stat);
  } else {
    (void) sndstat_random(in, sfinfo, "iamix audio file", RandomSampleTime, RandomSampleSize, &stat);
  }
  sf_seek(in, 0, SEEK_SET);	// Rewind to the start of the snd
  return stat.std();
}  // std_snd()


//////////////////////////////////////////////////////////////////////
//
// Error checking versions of atof and atoi
//

float my_atof(char* in) {
  float out;
  if (sscanf(in, "%f", &out) != 1) {
    usage();
  }
  return out;
}  // my_atof()

int my_atoi(char* in) {
  int out;
  if (sscanf(in, "%d", &out) != 1) {
    usage();
  }
  return out;
}  // my_atoi()


