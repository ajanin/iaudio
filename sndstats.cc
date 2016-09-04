//////////////////////////////////////////////////////////////////////
//
// File: sndstats.cc
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Given an open sound file, compute simple statistics on it using the
// jstat class.
//

#include <stdlib.h>
#include <stdio.h>
#include <values.h>
#include <math.h>
#include <sys/time.h>

#include <assert.h>

#include <sndfile.h>

#include "sndstats.h"

extern char *ProgName;

//
// Read the entire file (except for optional margins at the start and
// end). Returns a jstat instance. If the passed stat is 0, allocate
// and return a jstat instance that the user must delete.
//

jstat* sndstat(SNDFILE* in, SF_INFO* info, const char* fname, 
	       float skip_time, float end_time, jstat* stat) {
  const long blocksize = 10000;
  long nread;
  float buf[blocksize];
  int i;
  long skip_frames;
  long end_frame;
  long cur_frame = 0;

  if (stat == 0) {
    stat = new jstat();
  }

  skip_frames = (long) (info->samplerate * skip_time);
  end_frame =   (long) (info->samplerate * end_time);

  if (skip_frames > 0) {
    if (sf_seek(in, skip_frames, SEEK_SET) == -1) {
      fprintf(stderr, "%s: seek failed for file %s.\n", ProgName, fname);
      exit(1);
    }
    cur_frame = skip_frames;
  }
  
  do {
    nread = sf_read_float(in, buf, blocksize);
    for (i = 0; i < nread; i++) {
      stat->datum(buf[i]);
    }
    cur_frame += nread;
  } while (nread == blocksize && (end_frame <= 0 || cur_frame < end_frame));

  return stat;
} // sndstat()


//
// Read a random sampling from the file.
// Returns a jstat instance. If the passed stat is 0, allocate
// and return a jstat instance that the user must delete.
//
// Initialized the random seed from the current time.
//

jstat* sndstat_random(SNDFILE* in, SF_INFO* info, const char* fname, float random_sample,
		      float random_size, jstat* stat) {
  long blocksize;
  long nread;
  float* buf;
  int i;
  long to_read;
  long random_frame;
  struct timeval tp;

  if (stat == 0) {
    stat = new jstat();
  }

  gettimeofday(&tp, NULL);
  srandom(tp.tv_usec);
  
  // Total number of frames to be read
  to_read = (long) (info->samplerate * random_sample); 

  // Number of frames in one sample
  blocksize = (long) (info->samplerate * random_size);
  buf = new float[blocksize];
  assert(buf);

  while (to_read > 0) {
    random_frame = ((long long) random()) * info->frames / INT_MAX;
    if (sf_seek(in, random_frame, SEEK_SET) == -1) {
      fprintf(stderr, "%s: seek failed for file %s.\n", ProgName, fname);
      exit(1);
    }
    nread = sf_read_float(in, buf, blocksize);
    for (i = 0; i < nread; i++) {
      stat->datum(buf[i]);
    }
    to_read -= nread;
  }
  delete [] buf;
  
  return stat;
}  // sndstat_random()

//////////////////////////////////////////////////////////////////////
//
// jstat methods
//

jstat::jstat() {
  clear();
}  //  jstat()

void jstat::clear() {
  sum_ = 0.0;
  sum2_ = 0.0;
  n_ = 0;
  min_ = MAXDOUBLE;
  max_ = MINDOUBLE;
}  // clear

void jstat::datum(double val) {
  sum_ += val;
  sum2_ += val*val;
  n_++;
  if (val < min_) min_ = val;
  if (val > max_) max_ = val;
}  //  datum

double jstat::mean() {
  if (n_ <= 0) {
    fprintf(stderr, "Not enough data to determine mean\n");
    exit(1);
  }
  return (sum_ / n_);
}  // mean()

double jstat::std() {
  if (n_ <= 1) {
    fprintf(stderr, "Not enough data to determine standard deviation\n");
    exit(1);
  }
  return sqrt((sum2_ - (sum_ * sum_ / n_) ) / (n_ - 1));
}  //  std()

double jstat::min() {
  if (n_ <= 0) {
    fprintf(stderr, "Not enough data to determine minimum\n");
    exit(1);
  }
  return min_;
}  // min()

double jstat::max() {
  if (n_ <= 0) {
    fprintf(stderr, "Not enough data to determaxe maximum\n");
    exit(1);
  }
  return max_;
}  // max()

int jstat::n() {
  return n_;
}  // n()
