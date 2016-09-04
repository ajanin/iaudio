//////////////////////////////////////////////////////////////////////
//
// File: iastat.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.

//
// Report min, max, mean, stddev, variance, and/or inverse stddev of a
// sound file.
//
//  02/22/00 Adam Janin
//     Original version using Dan Ellis's dwpelib
//  02/09/05 Adam Janin
//     Converted to use libsndfile instead of Dan Ellis's dpwelib.
//     Renamed to iastat
//


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sndfile.h>

#include "sndstats.h"

//////////////////////////////////////////////////////////////////////
//
// Globals
//

char* ProgName;


static int show_mean;
static int show_max;
static int show_min;
static int show_n;
static int show_std;
static int show_var;
static int show_istd;
static int show_labs;
static float skip_time;
static float end_time;
static float random_sample;
static float random_size;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();

static float my_atof(const char*);
static void print_results(jstat&);

//////////////////////////////////////////////////////////////////////
//
// Main
//

int main(int argc, char** argv) {
  SNDFILE* in;
  SF_INFO info;
  int c;
  jstat stat;
  extern char *optarg;
  extern int optind;
  
  ProgName = argv[0];

  show_mean = 0;
  show_max  = 0;
  show_min  = 0;
  show_n    = 0;
  show_std  = 0;
  show_var  = 0;
  show_istd = 0;
  show_labs = 0;
  skip_time = 0.0;
  end_time = -1.0;
  random_sample = -1.0;
  random_size = 1.0;
    
  while ((c = getopt(argc, argv, "dDe:k:lmnNr:R:vx")) != EOF) {
    switch (c) {
    case 'd':
      show_std  = 1;
      break;
    case 'D':
      show_istd = 1;
      break;
    case 'e':
      end_time = my_atof(optarg);
      break;
    case 'k':
      skip_time = my_atof(optarg);
      break;
    case 'l':
      show_labs = 1;
      break;
    case 'm':
      show_mean = 1;
      break;
    case 'n':
      show_min  = 1;
      break;
    case 'N':
      show_n    = 1;
      break;
    case 'r':
      random_sample = my_atof(optarg);
      break;
    case 'R':
      random_size = my_atof(optarg);
      break;
    case 'v':
      show_var  = 1;
      break;
    case 'x':
      show_max  = 1;
      break;
    }
  }

  if (optind >= argc) {
    usage();
  }

  if (random_sample > 0.0 && (skip_time > 0.0 || end_time > 0.0)) {
    usage();
  }
  
  for (; optind < argc; optind++) {
    
    // I should really have command line args for headerless formats...
    
    if ((in = sf_open(argv[optind], SFM_READ, &info)) == NULL) {
      fprintf(stderr, "%s: couldn't open '%s' as input sound file\n",
	      ProgName, argv[optind]);
      usage();
    }
    
    if (info.channels != 1) {
      fprintf(stderr, "%s: %s is a multi channel sound files. Not (yet) supported.\n",
	      ProgName, argv[optind]);
      exit(1);
    }

    if ((random_sample > 0.0 || skip_time > 0) && !info.seekable) {
      fprintf(stderr, "%s: %s is not seekable. Try setting random_sample to 0 and/or skip_time to 0.\n",
	      ProgName, argv[optind]);
      exit(1);
    }
    
    if (random_sample > 0.0) {
      sndstat_random(in, &info, argv[optind], random_sample, random_size, &stat);
    } else {
      sndstat(in, &info, argv[optind], skip_time, end_time, &stat);
    }

    sf_close(in);
  }
  print_results(stat);
  
  return 0;
}

void usage() {
  fprintf(stderr, "Usage: %s -mxnNdvDl -k # -e # -r # -R # infile\n", ProgName);
  fprintf(stderr, " m - mean\n x - max\n n - min\n N - number of points\n");
  fprintf(stderr, " d - std\n v - variance\n D - 1/std\n l - show labels\n");
  fprintf(stderr, " k N - skip N seconds before starting analysis\n");
  fprintf(stderr, " e N - stop after about N seconds (this is approximate)\n");
  fprintf(stderr, " r N - sample for a total of N seconds randomly\n");
  fprintf(stderr, " R N - random sample size of N seconds (defaults to 1.0)\n");
  fprintf(stderr, "\nNote: -r cannot be specified with -k or -e\n");
  exit(1);
}

static float my_atof(const char* in) {
  float out;
  if (sscanf(in, "%f", &out) != 1) {
    fprintf(stderr, "Bad float value %s\n", in);
    usage();
  }
  return out;
}

static void print_results(jstat& stat) {
  if (show_mean) {
    if (show_labs) {
      printf("  Mean: ");
    }
    printf("%f", stat.mean());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_min) {
    if (show_labs) {
      printf("   Min: ");
    }
    printf("%f", stat.min());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_max) {
    if (show_labs) {
      printf("   Max: ");
    }
    printf("%f", stat.max());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_n) {
    if (show_labs) {
      printf("     N: ");
    }
    printf("%d", stat.n());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_std) {
    if (show_labs) {
      printf("   Std: ");
    }
    printf("%f", stat.std());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_var) {
    if (show_labs) {
      printf("   Var: ");
    }
    printf("%f", stat.std() * stat.std());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }

  if (show_istd) {
    if (show_labs) {
      printf("  IStd: ");
    }
    printf("%f", 1.0 / stat.std());
    if (show_labs) {
      printf("\n");
    } else {
      printf(" ");
    }
  }
  printf("\n");
}

