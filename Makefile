# See LICENSE file for copyright and license details.

CXX = c++
CXXFLAGS = -std=c++11
LDFLAGS = -lcurses

mte : Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o mte mte.cc
