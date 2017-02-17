#!/usr/bin/env python
#
# Script that "inverts" a file containing start/end times. The input file must be sorted and must
# not contain overlaps.
#
# E.g. if the input is:
#
#  10.2  15.6
#  16.2  30.5
#  35.1  50.2
#
# then 'between.py -e 60 < input' will produce:
#
#  0 10.2
#  15.6 16.2
#  30.5 35.1
#  50.2 60

from __future__ import print_function

from six.moves import input
import six

import argparse
import logging
import re
import sys

VERSION = 0.1

class Global:
    '''Stores globals. There should be no instances of Global.'''

    # Command line arguments
    args = None

    # What is considered a blank line.
    comment_re = re.compile('^\s*(#.*)?$')

# end class Global

def main(argv):
    parse_arguments(argv[1:])
    setup_logging()

    # Read the first non-comment line that's after start.
    for line in sys.stdin:
        if Global.comment_re.match(line):
            continue
        (s,e) = map(float, line.split())
        if e > Global.args.filestarttime:
            break

    if s > Global.args.filestarttime:
        print(Global.args.filestarttime, s)
    prevend = e

    # Now loop through until end.
    for line in sys.stdin:
        if Global.comment_re.match(line):
            continue
        (s,e) = map(float, line.split())
        if Global.args.fileendtime is not None and s >= Global.args.fileendtime:
            break
        if s > prevend:
            print(prevend, s)
        prevend = e

    if Global.args.fileendtime is not None and prevend < Global.args.fileendtime:
        print(prevend, Global.args.fileendtime)
        
    
# end main()
              
def parse_arguments(strs):
    parser = argparse.ArgumentParser(description='Description. Version %s.'%(VERSION))
    parser.add_argument('-s', dest='filestarttime', type=float, default=0.0, help='Start time to use (default %(default)s')
    parser.add_argument('-e', dest='fileendtime', type=float, help='End time to use. If not provided, use the final time in the input file.')
    parser.add_argument('-loglevel', 
                        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                        default='WARNING',
                        help='Logging level (default %(default)s)')
    parser.add_argument('-version', '--version', action='version', version=str(VERSION))
    Global.args = parser.parse_args(strs)
# end parse_arguments()

def setup_logging():
    numeric_level = getattr(logging, Global.args.loglevel, None)
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s' % loglevel)
    logging.basicConfig(level=numeric_level, format="%(module)s:%(levelname)s: %(message)s")
# end setup_logging()

if __name__ == "__main__":
    main(sys.argv)

