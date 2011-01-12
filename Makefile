# Makefile for C++ project with auto-dependencies and multiple build options
# Copyright (C) 2000-2003 Marc Mongenet

###############################################################################
# USER DEFINED VARIABLES
# The following variable values must be suited to the build environment.
###############################################################################

# name of the file to build
EXE = a.out

# C++ compiler
CXX = g++ -std=c++0x

# C compiler
CC = gcc

URL = http://127.0.0.1:8081/index.html

#FIXME: Make this a config string
BROWSER = /home/mikola/Apps/chrome-linux/chrome --enable-webgl

# source files directory
srcdir = src

# build directory
builddir = obj


# preprocessor options to find all included files
INC_PATH = -I$(srcdir) -I/usr/local/include

# libraries link options ('-lm' is common to link with the math library)
LNK_LIBS = -L/usr/local/lib -ltokyocabinet -lz -lbz2 -lrt -pthread -ldl -lm -lc

# other compilation options
COMPILE_OPTS = -pthread

# basic compiler warning options (for GOAL_EXE)
BWARN_OPTS = 

# extented compiler warning options (for GOAL_DEBUG)
EWARN_OPTS = $(BWARN_OPTS) -pedantic\
	     -Wtraditional -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align\
	     -Wwrite-strings -Wstrict-prototypes -Wredundant-decls\
	     -Wnested-externs -Woverloaded-virtual -Winline

###############################################################################
# INTERNAL VARIABLES
# The following variable values should not depend on the build environment.
# You may safely stop the configuration at this line.
###############################################################################

# You may freely change the following goal names if you dislike them.
GOAL_DEBUG = debug
GOAL_PROF = prof
GOAL_EXE = all

# build options for GOAL_DEBUG (executable for debugging) goal
ifeq "$(MAKECMDGOALS)" "$(GOAL_DEBUG)"

 # specific options for debugging
 GOAL_OPTS = -g
 # compilation verification options
 WARN_OPTS = $(BWARN_OPTS)
 # optimization options
 OPTIMISE_OPTS =
 # dependencies must be up to date
 CHECK_DEPS = yes

else

 # build options for GOAL_PROF (executable for profiling) goal
 ifeq "$(MAKECMDGOALS)" "$(GOAL_PROF)"

  # specific options for profiling
  GOAL_OPTS = -pg
  # compilation verification options
  WARN_OPTS = $(BWARN_OPTS)
  # optimization options
  OPTIMISE_OPTS = -O2
  # dependencies must be up to date
  CHECK_DEPS = yes

 else

  # build options for GOAL_EXE (optimized executable) goal
  ifeq "$(MAKECMDGOALS)" "$(GOAL_EXE)"

   # specific options for optimized executable
   GOAL_OPTS = -s
   # compilation verification options
   WARN_OPTS = $(BWARN_OPTS)
   # optimization options
   OPTIMISE_OPTS = -O3 -fomit-frame-pointer
   # dependencies must be up to date
   CHECK_DEPS = yes

  else

   # Other goals do not require up to date dependencies.
   CHECK_DEPS = no

  endif
 endif
endif

# preprocessor options
CPPOPTS = $(INC_PATH)

# compiler options
CXXOPTS = $(GOAL_OPTS) $(COMPILE_OPTS) $(WARN_OPTS) $(OPTIMISE_OPTS)

# linker options
LDOPTS = $(GOAL_OPTS) $(LNK_LIBS)

# source files in this project
cppsources := $(wildcard $(srcdir)/*.cc)
csources := $(wildcard $(srcdir)/*.c)
sources := $(cppsources) $(csources)

# object files in this project
cppobjs := $(notdir $(cppsources))
cppobjs := $(addprefix $(builddir)/, $(cppobjs))
cobjs := $(notdir $(csources))
cobjs := $(addprefix $(builddir)/, $(cobjs))
objs := $(cppobjs:.cc=.o) $(cobjs:.c=.o)

# executable with full path
exe = $(EXE)

# This makefile creates and includes makefiles containing actual dependencies.
# For every source file a dependencies makefile is created and included.
# The deps variable contains the list of all dependencies makefiles.
deps_suffix = d
deps := $(objs:.o=.$(deps_suffix))

# To detect goal changes (for instance from GOAL_DEBUG to GOAL_EXE)
# between invocations, this makefile creates an empty file (the goal flag
# file) which suffix is the goal name.
goal_flag_file_prefix = $(builddir)/Last_make_goal_was_
goal_flag_file = $(goal_flag_file_prefix)$(MAKECMDGOALS)

###############################################################################
# TARGETS
###############################################################################

# Delete the target file of a rule if the command used to update it failed.
# Do that because the newly generated target file may be corrupted but appear
# up to date.
.DELETE_ON_ERROR:

# Clear default suffix list to disable all implicit rules.
.SUFFIXES:

# usage message for this makefile
.PHONY:	usage
usage:
	@echo "GOAL	EFFECT"
	@echo "----	------"
	@echo "usage	print this message"
	@echo "list	list the source files"
	@echo "$(GOAL_EXE)	build the executable"
	@echo "$(GOAL_DEBUG)	build the executable with debug options"
	@echo "$(GOAL_PROF)	build the executable with profiling options"
	@echo "clean	remove all built files"

# If source files exist then build the EXE file.
.PHONY:	$(GOAL_EXE)
ifneq "$(strip $(sources))" ""
$(GOAL_EXE):	$(exe)
else
$(GOAL_EXE):
	@echo "No source file found."
endif

# GOAL_DEBUG and GOAL_PROF targets use the same rules than GOAL_EXE.
.PHONY:	$(GOAL_DEBUG)
$(GOAL_DEBUG):	$(GOAL_EXE)

.PHONY:	$(GOAL_PROF)
$(GOAL_PROF):	$(GOAL_EXE)

###############################################################################
# BUILDING
# Note: CPPFLAGS, CXXFLAGS or LDFLAGS are not used but may be specified by the
# user at make invocation.
###############################################################################

test: $(exe)
	(sleep 2; $(BROWSER) $(URL)) &
	./$(exe)


# linking
$(exe):	$(objs)
	$(CXX) $^ -o $@ $(LDOPTS) $(LDFLAGS)

# explicit definition of the implicit rule used to compile source files
$(builddir)/%.o:	src/%.cc
	$(CXX) -c $< $(CPPOPTS) $(CXXOPTS) $(CPPFLAGS) $(CXXFLAGS) -o $@

$(builddir)/%.o:	src/%.c
	$(CC) -c $< $(CPPOPTS) $(CXXOPTS) $(CPPFLAGS) $(CXXFLAGS) -o $@

# Rule to build our included dependencies makefiles.
# This rule is used by GNU Make because it automatically tries to (re)build
# obsolete or non-existent included makefiles.
# These files are created with one line of the form:
# 1.o 1.d: $(goal_flag_file) 1.cc 1.h 2.h 3.h g.h
# The implicit rule previously defined will be used for compilation.
# Note that the dependencies can be goal specific.
# The goal_flag_file is determined at run time because it must be the current
# goal and not the goal in use when the dependencies makefile was created.
$(builddir)/%.$(deps_suffix):	src/%.cc $(goal_flag_file)
	$(SHELL) -ec '$(CXX) -MM $(CPPOPTS) $(CPPFLAGS) $< |\
	sed '\''s@\($*\)\.o[ :]*@$(builddir)/\1.o $@: $$(goal_flag_file) @g'\'' > $@;\
	[ -s $@ ] || rm -f $@'


$(builddir)/%.$(deps_suffix):	src/%.c $(goal_flag_file)
	$(SHELL) -ec '$(CC) -MM $(CPPOPTS) $(CPPFLAGS) $< |\
	sed '\''s@\($*\)\.o[ :]*@$(builddir)/\1.o $@: $$(goal_flag_file) @g'\'' > $@;\
	[ -s $@ ] || rm -f $@'


# If dependencies have to be up to date then include dependencies makefiles.
ifeq "$(CHECK_DEPS)" "yes"
 ifneq "$(strip $(sources))" ""
  include $(deps)
 endif
endif

# Rule to produce the goal flag file.
# If the goal has changed then we must rebuild on a clean state because
# pre-processor DEFINE's may have changed.
$(goal_flag_file):
	rm -f $(exe) $(goal_flag_file_prefix)* $(objs) $(deps)
	touch $@

###############################################################################
# NON-BUILD TARGETS
###############################################################################

# List the source files
.PHONY:	list
list:
	@echo $(sources) | tr [:space:] \\n

# Remove all files that are normally created by building the program.
.PHONY:	clean
clean:
	rm -f $(exe) $(goal_flag_file_prefix)* $(objs) $(deps)
