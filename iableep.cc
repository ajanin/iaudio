//////////////////////////////////////////////////////////////////////
//
// File: iableep.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Given an audio file and a list of start end times, write a new
// audio file with the specified times "bleeped" out.
//
// 10/29/00 Adam Janin
//   Original version. Uses Dan Ellis's libdpwe routines.
//
// 02/10/05 Adam Janin
//   Renamed iableep and changed to use libsndfile.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include <sndfile.h>

#include "sndstats.h"

#define BUFSIZE (32000)
#define LINESIZE (1024)

//////////////////////////////////////////////////////////////////////
//
// Globals
//

char* ProgName;

static int Verbose = 0;
static float Amplitude = -1.0;		// If -1, compute automatically
static float Frequency = 440.0;

SNDFILE* InSound;
SF_INFO  InInfo;

SNDFILE* OutSound;
SF_INFO  OutInfo;

FILE* BleepFile;

long TotalNFrames;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();
static char* my_strdup(const char* in);
static float my_atof(const char* in);

static void sndbleep();

static float sndin();
static void sndout(float);
static void sndoutflush();

static char* getline(FILE*);
static int isblank(char*);

//////////////////////////////////////////////////////////////////////
//
// Main
//

int main(int argc, char** argv) {
  ProgName = argv[0];

  extern char *optarg;
  extern int optind;

  int c;
  
  const char* outfn = "-";		// Output file name
  const char* infn = "-";
  const char* bleepfn = "-";
    
  while ((c = getopt(argc, argv, "a:f:stvb:i:o:")) != EOF) {
    switch (c) {
    case 'a':
      Amplitude = my_atof(optarg);
      break;
    case 'f':
      Frequency = my_atof(optarg);
      break;
    case 'v':
      Verbose = 1;
      break;
    case 'b':
      bleepfn = my_strdup(optarg);
      break;
    case 'i':
      infn = my_strdup(optarg);
      break;
    case 'o':
      outfn = my_strdup(optarg);
      break;
    default:
      usage();
      break;
    }
  }

  if (optind != argc) {
    usage();
  }

  if (infn[0] == '-' && bleepfn[0] == '-') {
    fprintf(stderr, "%s: Input sound file and bleep file are both stdin!\n", ProgName);
    usage();
  }
    
  InSound = sf_open(infn, SFM_READ, &InInfo);
  if (InSound == NULL) {
    fprintf(stderr, "%s: couldn't open '%s' as input sound file\n",
	    ProgName, infn);
    usage();
  }
  if (InInfo.channels != 1) {
    fprintf(stderr, "%s: '%s' has %d channels; only 1 channel is supported.\n",
	    ProgName, infn, InInfo.channels);
    usage();
  }

  memcpy(&OutInfo, &InInfo, sizeof(SF_INFO));

  OutSound = sf_open(outfn, SFM_WRITE, &OutInfo);
  if (!OutSound) {
    fprintf(stderr, "%s: cannot open '%s' as output sound file\n",
	    ProgName, outfn);
    usage();
  }
  
  TotalNFrames = InInfo.frames;

  if (bleepfn[0] == '-') {
    BleepFile = stdin;
  } else {
    if ((BleepFile = fopen(bleepfn, "r")) == NULL) {
      fprintf(stderr, "%s: couldn't open bleepfile '%s'\n", ProgName, bleepfn);
      usage();
    }
  }

  // Automatically set the amplitude to the stddev of the signal if
  // amp < 0.

  if (Amplitude < 0.0) {
    if (Verbose) {
      fprintf(stderr, "Computing amplitude...");
    }
    jstat stat;
    // Hard wired for 5 minutes in 10 second chunks
    sndstat_random(InSound, &InInfo, infn, 300.0, 10.0, &stat);
    if (sf_seek(InSound, 0, SEEK_SET) == -1) {
      fprintf(stderr, "%s: couldn't rewind input file %s\n", ProgName, infn);
      usage();
    }
    Amplitude = stat.mean() + stat.std();
    if (Verbose) {
      fprintf(stderr, "\nAmplitude = %f\n", Amplitude);
    }
  }

  // do the work

  sndbleep();
      
  
  sf_close(OutSound);
  sf_close(InSound);
  if (BleepFile != stdin) {
    fclose(BleepFile);
  }
  
  return 0;
} // main()

static char* my_strdup(const char* in) {
  char* out;
  out = new char[strlen(in)+1];
  assert(out);
  strcpy(out, in);
  return out;
}

static float my_atof(const char* in) {
  float out;
  if (sscanf(in, "%f", &out) != 1) {
    usage();
  }
  return out;
}


void usage() {
  fprintf(stderr, "\nUsage: %s -a amp -f freq -v -i input -o output -b bleepfile\n",
	  ProgName);
  fprintf(stderr, "  -v		Verbose\n");
  fprintf(stderr, "  -i input   Input sound file [-]\n");
  fprintf(stderr, "  -o output  Output sound file [-]\n");
  fprintf(stderr, "  -b bleep   Bleep start/end time pairs [-]\n");
  fprintf(stderr, "  -a amp     Amplitude of tone [stddev]\n");
  fprintf(stderr, "  -f freq    Frequency of tome [440]\n");
  fprintf(stderr, "\n");
  exit(1);
}

static void sndbleep() {
  char* line;
  long startframe, endframe;
  long i;
  long curframe;
  float starttime, endtime;
  float sr = InInfo.samplerate;
  float fm = 2.0 * M_PI * Frequency / sr;
  
  curframe = 0;
  while ((line = getline(BleepFile)) != NULL) {
    if (sscanf(line, "%f %f", &starttime, &endtime) != 2) {
      fprintf(stderr, "%s: '%s', bad start end pair in bleepfile\n",
	      ProgName, line);
      usage();
    }
    startframe = (long) (starttime * sr);
    endframe =   (long) (endtime   * sr);

    if (Verbose) {
      fprintf(stderr, "Processing unedited block %ld - %ld\n", curframe, startframe-1);
    }
    
    for (i = curframe; i < startframe; i++) {
      sndout(sndin());
    }

    if (Verbose) {
      fprintf(stderr, "Bleeping out block %ld - %ld\n", startframe, endframe);
    }

    for (i = startframe; i <= endframe; i++) {    
      sndout(Amplitude * sin(fm*(i-startframe)));
      (void) sndin();	// Discard matching.
    }
    curframe = endframe+1;
  }
  
  if (Verbose) {
    fprintf(stderr, "Processing unedited block %ld - %ld\n", curframe, TotalNFrames);
  }
  for (i = curframe; i < TotalNFrames; i++) {
    sndout(sndin());
  }
  if (Verbose) {
    fprintf(stderr, "Flushing output\n");
  }
  sndoutflush();
}

// Needs to be globsl so sndoutflush can flush it.
float OutBuf[BUFSIZE];
int OutBufIndex = 0;

void sndout(float v) {
  OutBuf[OutBufIndex++] = v;
  if (OutBufIndex >= BUFSIZE) {
    sf_write_float(OutSound, OutBuf, BUFSIZE);
    OutBufIndex = 0;
  }
}

void sndoutflush() {
  sf_write_float(OutSound, OutBuf, OutBufIndex);
}

  
float sndin() {
  static float inbuf[BUFSIZE];
  static int index = 0;
  static int end = -1;
  
  if (end <= index) {
    end = sf_read_float(InSound, inbuf, BUFSIZE);
    //fprintf(stderr, "read %d bytes\n", end);
    if (end <= 0) {
      fprintf(stderr, "%s: sndin read %d bytes!\n", ProgName, end);
      //      usage();
    }
    index = 0;
  }
  return inbuf[index++];
}  // sndin()

char* getline(FILE* fp) {
  static char* line = 0;
  if (line == 0) {
    line = new char[LINESIZE];
    assert(line);
  }
  do {
    line = fgets(line, LINESIZE-1, fp);
  } while (line && (line[0] == '#' || isblank(line)));
  return line;
}

int isblank(char* line) {
  while (*line) {
    if (!isspace(*line++)) {
      return 0;
    }
  }
  return 1;
}
