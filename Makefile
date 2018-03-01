# See LICENSE file for copyright and license details.

CXX = c++
CXXFLAGS = -std=c++11 -Wall -Werror
LDFLAGS = -lcurses

mte : Makefile mte.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o mte mte.cc
