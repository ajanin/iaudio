////////////////////////////////////////////////////////////////////
//
// File: iainfo.cc
// Version: 0.1.0
// Author: Adam Janin
//
// Copyright 2014 International Computer Science Institute
// See the file LICENSE for licensing terms.
//
// Report information on sound files (e.g. format, length, etc).
// Information reported here does not require reading the entire
// audio file -- just the header. See iastat for statistics that
// require reading the entire audio file.
//
//  04/13/05 Adam Janin
//   Original version inspired by sndfile-info.c by Erik de Castro
//   Lopo.
//
//  03/28/06 Adam Janin
//   Switched to using a printf-like format
//
//  03/16/07 Adam Janin
//   Added option of number of channels
//
//  03/31/09 Adam Janin
//   Fixed default.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sndfile.h>

//////////////////////////////////////////////////////////////////////
//
// Defines
//

#define MEMCHECK(v_)						    \
  if (!v_) { 							    \
    fprintf(stderr, "%s: Allocation failed, line %d, file %s\n",    \
	    ProgName, __LINE__, __FILE__);			    \
    exit(-1);							    \
  }

#define VERSION_STRING "0.1.0"

//////////////////////////////////////////////////////////////////////
//
// Globals
//

char* ProgName;

char* Format;
const char* FormatDefault = "%f\\n Duration: %d seconds\\t%D frames\\n Sample Rate: %s Hz\\n";

const char* Filename;
double Duration;
int SampleRate;
int NSamples;
int NChannels;

//////////////////////////////////////////////////////////////////////
//
// Prototypes
//

void usage();
void iainfo(const char* fname);
void do_format(char);
void do_backslash(char);

//////////////////////////////////////////////////////////////////////
//
// Main
//

int main(int argc, char** argv) {
  int c, i;
  extern char *optarg;
  extern int optind;

  ProgName = argv[0];
  Format = 0;

  while ((c = getopt(argc, argv, "f:hV")) != EOF) {
    switch (c) {
    case 'f':
      if (Format) free(Format);
      Format = strdup(optarg);
      MEMCHECK(Format);
      break;
    case 'V':
      fprintf(stderr, "%s version %s\n", ProgName, VERSION_STRING);
      exit(1);
    case 'h':
    default:
      usage();
      break;
    }
  }

  if (!Format) {
    Format = strdup(FormatDefault);
    MEMCHECK(Format);
  }

  if (optind == argc) {
    iainfo("-"); // If no filename, use stdin
  } else {
    for (i = optind; i < argc; i++) {
      iainfo(argv[i]);
    }
  }
  free(Format);
  return 0;
} // main()

void iainfo(const char* fname) {
  SNDFILE* in;
  SF_INFO info;
  char* f;
  
  if ((in = sf_open(fname, SFM_READ, &info)) == NULL) {
    fprintf(stderr, "%s: couldn't open '%s' as input sound file\n",
	    ProgName, fname);
    usage();
  }

  Filename = fname;
  NSamples = info.frames;
  SampleRate = info.samplerate;
  Duration = (double) info.frames / info.samplerate;
  NChannels = info.channels;
    
  for (f = Format; *f != '\0'; f++) {
    if (*f == '%') {
      f++;
      if (*f == '\0') {
	putchar(*f);
	break;
      }
      do_format(*f);
    } else if (*f == '\\') {
      f++;
      if (*f == '\0') {
	putchar(*f);
	break;
      }
      do_backslash(*f);
    } else {
      putchar(*f);
    }
  }
  
  sf_close(in);
}  // iainfo()

void do_backslash(char f) {
  switch (f) {
  case '\\':
    putchar('\\');
    break;
  case 'n':
    printf("\n");
    break;
  case 't':
    printf("\t");
    break;
  default:
    fprintf(stderr, "%s: Unrecognized backslash '\\%c'\n", ProgName, f);
    exit(-1);
  }
}
// do_backslash()
 

void do_format(char f) {
  switch (f) {
  case 'c':
    printf("%d", NChannels);
    break;
  case 'd':
    printf("%f", Duration);
    break;
  case 'D':
    printf("%d", NSamples);
    break;
  case 's':
    printf("%d", SampleRate);
    break;
  case 'f':
    printf("%s", Filename);
    break;
  case '%':
    putchar('%');
    break;
  default:
    fprintf(stderr, "%s: Unrecognized format '%%%c'\n", ProgName, f);
    exit(-1);
  }
} // do_format()

void usage() {
  fprintf(stderr, "\nUsage: %s -f format infile1 infile2 ...\n", ProgName);
  fprintf(stderr, " -f format  -  Format string. See below.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Report information on sound files (e.g. duration, sample rate).\n");
  fprintf(stderr, "Information reported here does not require reading the entire\n");
  fprintf(stderr, "audio file -- just the header. See iastat for statistics that\n");
  fprintf(stderr, "require reading the entire audio file.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "The format argument controls what is printed for each file.\n");
  fprintf(stderr, "It works somewhat like a printf string, where %%C is replaced\n");
  fprintf(stderr, "with a value from each file. The following control characters\n");
  fprintf(stderr, "are recognized:\n\n");
  fprintf(stderr, "\t%%c - The number of channels\n");
  fprintf(stderr, "\t%%d - The duration is seconds\n");
  fprintf(stderr, "\t%%D - The duration in number of samples\n");
  fprintf(stderr, "\t%%s - The sample rate in Hz\n");
  fprintf(stderr, "\t%%f - The file name (or - for stdin)\n");
  //  fprintf(stderr, "\t%% - \n");
  fprintf(stderr, "\t%%%% - The character '%%'\n");
  fprintf(stderr, "\t\\n - Newline\n");
  fprintf(stderr, "\t\\t - Tab\n");
  fprintf(stderr, "\nAnything else is echoed verbatim.\n\n");
  fprintf(stderr, "If no format is provided, it defaults to:\n %s\n", FormatDefault);
  fprintf(stderr, "\n");
  fprintf(stderr, " Version %s\n", VERSION_STRING);
  fprintf(stderr, "\n");
  exit(1);
} // usage()
