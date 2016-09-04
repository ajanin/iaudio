//////////////////////////////////////////////////////////////////////
//
// File:: iajoin.c
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Given an input file containing input audio filenames, start
// times, and end times, extract the excerpts, combine them, and
// write them to a file.
//
// Depending on the file system, it may be much faster if the input
// file is sorted by the first field.
//
// Requires libsndfile from http://www.mega-nerd.com/libsndfile
//
// iajoin output.wav -i input.txt
// iajoin output.wav < input.txt
// iajoin -h
//
// TODO: 
//
// Add command line argument for a base directory that is prepending
// to any relative paths in the input file.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sndfile.h>

//////////////////////////////////////////////////////////////////////
//
// Globals
//

// BUFSIZE is used both as the number of integers to read at a time
// from audio files, and the maximum line length of an entry in
// the input file.

#define BUFSIZE (32000)

char* ProgName;

// Times in the input file are divided by this to arrive at
// seconds.

float TimeDenom = 1.0;	

// If 0, the input file is "file start end".
// If 1, the input file is "file start duration".

int UseDuration = 0;


int Verbosity = 0;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();
int read_line(FILE* fp, char* inname, float* startspec, float* endspec);

inline int min(int a, int b) { return (a<b)?a:b; }

int main(int argc, char** argv) {
  extern char *optarg;
  extern int optind;
  int c;
  SNDFILE* insnd;
  SNDFILE* outsnd;
  SF_INFO ininfo;
  SF_INFO outinfo;
  int buf[BUFSIZE];
  FILE* infp;
  int nread;
  sf_count_t startframe;
  sf_count_t endframe;
  sf_count_t toread;
  float startspec;
  float endspec;
  char inaudio[BUFSIZE];
  char previnaudio[BUFSIZE];
  char *outaudio;
  const char *infname;
    
  ProgName = argv[0];
  
  UseDuration = 0;
  infp = stdin;
  TimeDenom = 1.0;
  Verbosity = 0;
  infname = "-";
  
  while ((c = getopt(argc, argv, "dhi:t:v:")) != EOF) {
    switch (c) {
    case 'd':
      UseDuration = 1;
      break;
    case 'h':
      usage();
      break;
    case 'i':
      infname = strdup(optarg);
      if (strcmp(optarg, "-")) {
	infp = fopen(optarg, "r");
	if (!infp) {
	  fprintf(stderr, "%s Error: Unable to open input file '%s'\n",
		  ProgName, optarg);
	  perror(0);
	  exit(EXIT_FAILURE);
	}
      }
      break;
    case 't':
      if (sscanf(optarg, "%f", &TimeDenom) != 1) {
	fprintf(stderr, "%s Error: Bad time conversion argument -t %s\n", ProgName,optarg);
	exit(EXIT_FAILURE);
      }
      break;
    case 'v':
      if (sscanf(optarg, "%d", &Verbosity) != 1) {
	fprintf(stderr, "%s Error: Bad verbosity level -v %s\n", ProgName, optarg);
	exit(EXIT_FAILURE);
      }
      break;
    default:
      fprintf(stderr, "%s Error: Unknown argument '%c'\n\n", ProgName, c);
      usage();
    }
  }

  if (argc - optind != 1) {
    fprintf(stderr, "%s Error: Expected one argument, got %d.\n",
	    ProgName, argc-optind);
    usage();
  }

  outaudio = argv[optind];

  if (Verbosity >= 1) {
    fprintf(stderr, "%s%s -i %s -t %f %s\n\n", ProgName, 
	    (UseDuration ? " -d" : ""),
	    infname, TimeDenom, outaudio);
  }

  // Do the work
  
  outsnd = 0;
  insnd = 0;
  previnaudio[0] = 0;
  inaudio[0] = 0;

  while (read_line(infp, inaudio, &startspec, &endspec)) {
    
    // Don't open the audio file if it's already open.
    if (insnd == 0 || strcmp(inaudio, previnaudio)) {
      if (insnd != 0) {
	sf_close(insnd);
      }
      if ((insnd = sf_open(inaudio, SFM_READ, &ininfo)) == NULL) {
	fprintf(stderr, "%s Error: couldn't open input sound '%s'\n",
		ProgName, inaudio);
	exit(EXIT_FAILURE);
      }
    }
    strcpy(previnaudio, inaudio);

    // Create the outsnd. This will only happen the first time
    // through the loop.
    if (outsnd == 0) {
      memcpy(&outinfo, &ininfo, sizeof(SF_INFO));
      if ((outsnd = sf_open(outaudio, SFM_WRITE, &outinfo)) == NULL) {
	fprintf(stderr, "%s: couldn't open output file '%s'\n",
		ProgName, outaudio);
	exit(EXIT_FAILURE);
      }
    }

    // Round to nearest frame.
    startframe = (int) (0.5+startspec*ininfo.samplerate/TimeDenom);
    if (UseDuration) {
      endframe = (int) (0.5+(startspec+endspec)*ininfo.samplerate/TimeDenom);
    } else {
      endframe = (int) (0.5+endspec*ininfo.samplerate/TimeDenom);
    }

    if (Verbosity >= 2) {
      fprintf(stderr, " %s %d %d\n", inaudio, (int) startframe, (int) endframe);
    }
    
    if (sf_seek(insnd, startframe, SEEK_SET) == -1) {
      fprintf(stderr, "%s Error: Attempt to extract from out of bounds in file '%s', start=%f end=%f\n", ProgName, inaudio, startspec, endspec);
      exit(EXIT_FAILURE);
    }

    toread = endframe - startframe;
    while (toread > 0) {
      nread = sf_readf_int(insnd, buf, min(toread, BUFSIZE/ininfo.channels));
      if (nread <= 0) {
	fprintf(stderr, "%s Error: Attempt to extract from after end in file '%s', start=%f end=%f\n", ProgName, inaudio, startspec, endspec);
	exit(EXIT_FAILURE);
      }
      toread -= nread;
      sf_writef_int(outsnd, buf, nread);
    }
  }
  sf_close(insnd);
  sf_close(outsnd);
  
  if (infp != stdin) {
    fclose(infp);
  }
  return EXIT_SUCCESS;
}

int read_line(FILE* fp, char* inname, float* startspec, float* endspec) {
  char buf[BUFSIZE];
  if (fgets(buf, BUFSIZE, fp) == 0) {
    // EOF
    return 0;
  }

  if (sscanf(buf, "%s %f %f", inname, startspec, endspec) != 3) {
    fprintf(stderr, "%s Error: Unable to parse input line '%s'", ProgName, buf);
    exit(EXIT_FAILURE);
  }
  return 1;
}

void usage() {
  fprintf(stderr, "\nUsage: %s [-d] [-t 1.0] [-i infile.txt] outfile.wav\n\n", ProgName);
  fprintf(stderr, "Extract excerpts from audio files and merges them into outfile.wav.\n\n");
  fprintf(stderr, "The \"infile.txt\" argument is a plain text file with 3 fields:\n");
  fprintf(stderr, "  inputaudiopath starttime endtime\n\n");
  fprintf(stderr, "By default, the times are in seconds.\n\n");
  fprintf(stderr, "The program will likely run much faster if infile is sorted by the first field.\n\n");
  fprintf(stderr, "%s can handle any format supported by libsndfile, though the\n", ProgName);
  fprintf(stderr, "output format is always the same as the first input format.\n\n");
  fprintf(stderr, "If -d is given, the input file should contain durations instead of end times:\n");
  fprintf(stderr, "  inputaudiopath starttime duration\n\n");
  fprintf(stderr, "If -i is not provided or if \"-i -\" is used, the input file will\n");
  fprintf(stderr, "be read from stdin rather than a file.\n\n");
  fprintf(stderr, "Each of the input file times (or durations) is divided by the argument\n");
  fprintf(stderr, "to -t, if any. This allows units other than seconds to be used in the\n");
  fprintf(stderr, "input file.\n\n");
  
  exit(EXIT_FAILURE);
}
