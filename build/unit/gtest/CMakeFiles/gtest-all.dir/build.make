# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vagrant/libpaxos

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vagrant/libpaxos/build

# Include any dependencies generated for this target.
include unit/gtest/CMakeFiles/gtest-all.dir/depend.make

# Include the progress variables for this target.
include unit/gtest/CMakeFiles/gtest-all.dir/progress.make

# Include the compile flags for this target's objects.
include unit/gtest/CMakeFiles/gtest-all.dir/flags.make

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o: unit/gtest/CMakeFiles/gtest-all.dir/flags.make
unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o: ../unit/gtest/gtest-all.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o"
	cd /home/vagrant/libpaxos/build/unit/gtest && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/gtest-all.dir/gtest-all.cc.o -c /home/vagrant/libpaxos/unit/gtest/gtest-all.cc

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/gtest-all.dir/gtest-all.cc.i"
	cd /home/vagrant/libpaxos/build/unit/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/vagrant/libpaxos/unit/gtest/gtest-all.cc > CMakeFiles/gtest-all.dir/gtest-all.cc.i

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/gtest-all.dir/gtest-all.cc.s"
	cd /home/vagrant/libpaxos/build/unit/gtest && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/vagrant/libpaxos/unit/gtest/gtest-all.cc -o CMakeFiles/gtest-all.dir/gtest-all.cc.s

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.requires:
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.requires

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.provides: unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.requires
	$(MAKE) -f unit/gtest/CMakeFiles/gtest-all.dir/build.make unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.provides.build
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.provides

unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.provides.build: unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o

# Object files for target gtest-all
gtest__all_OBJECTS = \
"CMakeFiles/gtest-all.dir/gtest-all.cc.o"

# External object files for target gtest-all
gtest__all_EXTERNAL_OBJECTS =

unit/gtest/libgtest-all.a: unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o
unit/gtest/libgtest-all.a: unit/gtest/CMakeFiles/gtest-all.dir/build.make
unit/gtest/libgtest-all.a: unit/gtest/CMakeFiles/gtest-all.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX static library libgtest-all.a"
	cd /home/vagrant/libpaxos/build/unit/gtest && $(CMAKE_COMMAND) -P CMakeFiles/gtest-all.dir/cmake_clean_target.cmake
	cd /home/vagrant/libpaxos/build/unit/gtest && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/gtest-all.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
unit/gtest/CMakeFiles/gtest-all.dir/build: unit/gtest/libgtest-all.a
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/build

unit/gtest/CMakeFiles/gtest-all.dir/requires: unit/gtest/CMakeFiles/gtest-all.dir/gtest-all.cc.o.requires
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/requires

unit/gtest/CMakeFiles/gtest-all.dir/clean:
	cd /home/vagrant/libpaxos/build/unit/gtest && $(CMAKE_COMMAND) -P CMakeFiles/gtest-all.dir/cmake_clean.cmake
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/clean

unit/gtest/CMakeFiles/gtest-all.dir/depend:
	cd /home/vagrant/libpaxos/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/libpaxos /home/vagrant/libpaxos/unit/gtest /home/vagrant/libpaxos/build /home/vagrant/libpaxos/build/unit/gtest /home/vagrant/libpaxos/build/unit/gtest/CMakeFiles/gtest-all.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : unit/gtest/CMakeFiles/gtest-all.dir/depend

