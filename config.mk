ifeq (${OS},Windows_NT)
	CXX := my_cl.bat
	CC := my_cl.bat
else
	CXX := g++-8
	CC := gcc-8
	CXXFLAGS += -Wno-unused-parameter
endif
HAS_CHARCONV := false

BUILD := debug