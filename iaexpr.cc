//////////////////////////////////////////////////////////////////////
//
// File: iaexpr.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Using libeval from http://www.dutky.info/jeff/software/libeval,
// apply a function to every sample of an audio file.
//
// To compile this file, just make sure libeval.a and eval.h are
// findable, and then edit the Makefile, appending iaexpr.cc to
// SOURCES and iaexpr to EXECS.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sndfile.h>

extern "C" {
#include "eval.h"
}

#define BUFSIZE (32000)

char* ProgName;

void usage();


int main(int argc, char** argv) {
  SNDFILE* insnd;
  SNDFILE* outsnd;
  SF_INFO ininfo;
  SF_INFO outinfo;
  float buf[BUFSIZE];
  double rv;
  int nread;
  int ii;
  int err;

  ProgName = argv[0];

  if (argc != 4) {
    usage();
  }

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

  // Use same format for output as input.
  memcpy(&outinfo, &ininfo, sizeof(SF_INFO));
  if ((outsnd = sf_open(argv[2], SFM_WRITE, &outinfo)) == NULL) {
    fprintf(stderr, "%s: couldn't open output file '%s'\n",
	    ProgName, argv[2]);
    exit(EXIT_FAILURE);
  }

  while ((nread = sf_read_float(insnd, buf, BUFSIZE)) > 0) {
    for (ii = 0; ii < nread; ii++) {
      eval_set_var((char*)"x", (float) buf[ii]);
      err = eval(argv[3], &rv);
      if (err) {
	fprintf(stderr, "%s: eval error #%d: %s\n", ProgName, err, eval_error(err));
	exit(EXIT_FAILURE);
      }
      // Store in-place
      buf[ii] = rv;
    }
    sf_write_float(outsnd, buf, nread);
  }
  
  sf_close(outsnd);
  sf_close(insnd);
  
  return(EXIT_SUCCESS);
}


void usage() {
  fprintf(stderr, "\nUsage: %s insnd outsnd expr\n", ProgName);
  fprintf(stderr, "\nApply an arbitrary function to every sample in an audio file.\n");
  fprintf(stderr, "\nExample: iaexpr test.sph testout.sph 'sign(x)*abs(x)^0.8'\n");
  fprintf(stderr, "\nUses libeval from http://www.dutky.info/jeff/software/libeval to do the evaluation\n\n");
  exit(EXIT_FAILURE);
}

