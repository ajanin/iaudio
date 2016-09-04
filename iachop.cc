//////////////////////////////////////////////////////////////////////
//
// File: iachop.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Given an input file containing input audio filenames, start times,
// and end times, and an output file, extract the excerpts and write
// them to a file. The output file is always overwritten.
//
// The program will be faster if the input file is sorted.
//
// Requires libsndfile from http://www.mega-nerd.com/libsndfile
//
// TODO: 
//
// Add command line arguments for a base directory that is prepended
// to any relative paths for the input files and output files.
//
// By default, don't overwrite existing files. Add a command line flag
// to allow overwriting.
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

// If set, times less than 0 are set to 0 and times greater
// than the length of the file are set to the length of the file.

int ForceInrangeTimes = 0;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();
int read_line(FILE* fp, char* inname, float* startspec, float* endspec, char* outname);

inline int min(int a, int b) { return (a<b)?a:b; }

int main(int argc, char** argv) {
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
  char outaudio[BUFSIZE];
  extern char *optarg;
  extern int optind;
  int c;
  int nargs;
    
  ProgName = argv[0];

  while ((c = getopt(argc, argv, "fht:")) != EOF) {
    switch (c) {
    case 'f':
      ForceInrangeTimes = 1;
      break;
    case 'h':
      usage();
      break;
    case 't':
      if (sscanf(optarg, "%f", &TimeDenom) != 1) {
	fprintf(stderr, "%s Error: Bad time conversion argument -t %s\n", ProgName,optarg);
	exit(EXIT_FAILURE);
      }
      break;
    default:
      fprintf(stderr, "%s Error: Unknown argument '%c'\n\n", ProgName, c);
      usage();
    }
  }

  nargs = argc - optind;
    
  // iachop
  // iachop -
  if (nargs == 0 || (nargs == 1 && !strcmp(argv[optind], "-"))) {
    infp = stdin;

  // iachop infile.txt
  } else if (nargs == 1) {
    infp = fopen(argv[optind], "r");
    if (!infp) {
      fprintf(stderr, "%s Error: Unable to open input file '%s'\n",
	      ProgName, argv[optind]);
      perror(0);
      exit(EXIT_FAILURE);
    }
  } else {
    usage();
  }

  // Do the work
  
  outsnd = 0;
  insnd = 0;
  previnaudio[0] = 0;
  inaudio[0] = 0;

  while (read_line(infp, inaudio, &startspec, &endspec, outaudio)) {
    
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

    memcpy(&outinfo, &ininfo, sizeof(SF_INFO));
    if ((outsnd = sf_open(outaudio, SFM_WRITE, &outinfo)) == NULL) {
      fprintf(stderr, "%s: couldn't open output file '%s'\n",
	      ProgName, outaudio);
      exit(EXIT_FAILURE);
    }

    // Hmm. (int), or rint()?
    startframe = (int) (startspec*ininfo.samplerate/TimeDenom);
    endframe = (int) (endspec*ininfo.samplerate/TimeDenom);
    
    if (ForceInrangeTimes) {
      if (startframe < 0) {
	startframe = 0;
      }
      if (endframe > ininfo.frames) {
	endframe = ininfo.frames;
      }
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
    sf_close(outsnd);
  }
  sf_close(insnd);
  
  if (infp != stdin) {
    fclose(infp);
  }
  return EXIT_SUCCESS;
}

int read_line(FILE* fp, char* inname, float* startspec, float* endspec, char* outname) {
  char buf[BUFSIZE];
  if (fgets(buf, BUFSIZE, fp) == 0) {
    // EOF
    return 0;
  }

  if (sscanf(buf, "%s %f %f %s", inname, startspec, endspec, outname) != 4) {
    fprintf(stderr, "%s Error: Unable to parse input line '%s'", ProgName, buf);
    exit(EXIT_FAILURE);
  }
  return 1;
}

void usage() {
  fprintf(stderr, "\nUsage: %s -f -t timedenom infile.txt", ProgName);
  fprintf(stderr, "\n       %s -f -t timedenom < infile.txt\n\n", ProgName);
  fprintf(stderr, "Extract excerpts from audio files and write them to output files.\n\n");
  fprintf(stderr, "The \"infile\" argument is a plain text file with 4 fields:\n");
  fprintf(stderr, "  inputaudiopath starttime endtime outputaudiopath\n\n");
  fprintf(stderr, "By default, the times are in seconds. If -t timedenom is provided, all\n");
  fprintf(stderr, "times given in infile be divided by timedenom.\n\n");
  fprintf(stderr, "If times in infile are before the start or after the end of the audio\n");
  fprintf(stderr, "file, the program reports an error and exits UNLESS the -f option is\n");
  fprintf(stderr, "provided, in which case no error is reported and the output will be\n");
  fprintf(stderr, "truncated to the start/end of the input audio.\n\n");
  fprintf(stderr, "The program will likely run much faster if infile is sorted by the first field.\n\n");
  fprintf(stderr, "%s can handle any format supported by libsndfile, though the\n", ProgName);
  fprintf(stderr, "output format is always the same as the input format.\n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

//
