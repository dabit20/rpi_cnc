#   This is the RS274NGC interpreter extension for the DaBit3D 3D printer
#   Copyright 2017 DaBit <dabit@icecoldcomputing.com>,
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import sys
import time
import traceback

from interpreter import *
import emccanon

throw_exceptions = 1 # raises InterpreterException if execute() or read() fail

# M84: disable stepper motors. Currently this does nothing, but is included for compatibility
def m84(self, **words):
  return INTERP_OK
  
# M400 is a queue buster, and does nothing else
def m400(self, **words):
  yield INTERP_EXECUTE_FINISH
  

