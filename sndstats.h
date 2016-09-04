//////////////////////////////////////////////////////////////////////
//
// File: sndstats.h
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// 02/10/05  Adam Janin
//    Initial version. See sndstats.cc
//

#ifndef SNDSTATS_H
#define SNDSTATS_H

#include <sndfile.h>

class jstat {
public:
  
  jstat();

  void clear();		//  Reset everything

  void datum(double);	//  Add individual datum
  
  double mean();	//  Mean of data (or error if n=0)
  double std();		//  Standard deviation (or error if n < 2)
  double min();		//  Min of data (or error if n=0)
  double max();		//  Max of data (or error if n=0)
  int	 n();		//  Number of data points

private:

  double sum_;		//  Sum of data points
  double sum2_;		//  Sum of square of data points
  double min_;
  double max_;
  int    n_;  
};  //  class jstat  


jstat* sndstat(SNDFILE* in, SF_INFO* info, const char* fname, 
	       float skip_time = 0.0, float end_time = -1.0, jstat* stat = 0);

jstat* sndstat_random(SNDFILE* in, SF_INFO* info, const char* fname, float random_sample,
		      float random_size = 1.0, jstat* stat = 0);

#endif // SNDSTATS_H
