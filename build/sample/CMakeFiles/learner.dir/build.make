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
include sample/CMakeFiles/learner.dir/depend.make

# Include the progress variables for this target.
include sample/CMakeFiles/learner.dir/progress.make

# Include the compile flags for this target's objects.
include sample/CMakeFiles/learner.dir/flags.make

sample/CMakeFiles/learner.dir/learner.c.o: sample/CMakeFiles/learner.dir/flags.make
sample/CMakeFiles/learner.dir/learner.c.o: ../sample/learner.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object sample/CMakeFiles/learner.dir/learner.c.o"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/learner.dir/learner.c.o   -c /home/vagrant/libpaxos/sample/learner.c

sample/CMakeFiles/learner.dir/learner.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/learner.dir/learner.c.i"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/sample/learner.c > CMakeFiles/learner.dir/learner.c.i

sample/CMakeFiles/learner.dir/learner.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/learner.dir/learner.c.s"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/sample/learner.c -o CMakeFiles/learner.dir/learner.c.s

sample/CMakeFiles/learner.dir/learner.c.o.requires:
.PHONY : sample/CMakeFiles/learner.dir/learner.c.o.requires

sample/CMakeFiles/learner.dir/learner.c.o.provides: sample/CMakeFiles/learner.dir/learner.c.o.requires
	$(MAKE) -f sample/CMakeFiles/learner.dir/build.make sample/CMakeFiles/learner.dir/learner.c.o.provides.build
.PHONY : sample/CMakeFiles/learner.dir/learner.c.o.provides

sample/CMakeFiles/learner.dir/learner.c.o.provides.build: sample/CMakeFiles/learner.dir/learner.c.o

sample/CMakeFiles/learner.dir/application_config.c.o: sample/CMakeFiles/learner.dir/flags.make
sample/CMakeFiles/learner.dir/application_config.c.o: ../sample/application_config.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object sample/CMakeFiles/learner.dir/application_config.c.o"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/learner.dir/application_config.c.o   -c /home/vagrant/libpaxos/sample/application_config.c

sample/CMakeFiles/learner.dir/application_config.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/learner.dir/application_config.c.i"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/sample/application_config.c > CMakeFiles/learner.dir/application_config.c.i

sample/CMakeFiles/learner.dir/application_config.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/learner.dir/application_config.c.s"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/sample/application_config.c -o CMakeFiles/learner.dir/application_config.c.s

sample/CMakeFiles/learner.dir/application_config.c.o.requires:
.PHONY : sample/CMakeFiles/learner.dir/application_config.c.o.requires

sample/CMakeFiles/learner.dir/application_config.c.o.provides: sample/CMakeFiles/learner.dir/application_config.c.o.requires
	$(MAKE) -f sample/CMakeFiles/learner.dir/build.make sample/CMakeFiles/learner.dir/application_config.c.o.provides.build
.PHONY : sample/CMakeFiles/learner.dir/application_config.c.o.provides

sample/CMakeFiles/learner.dir/application_config.c.o.provides.build: sample/CMakeFiles/learner.dir/application_config.c.o

sample/CMakeFiles/learner.dir/net_utils.c.o: sample/CMakeFiles/learner.dir/flags.make
sample/CMakeFiles/learner.dir/net_utils.c.o: ../sample/net_utils.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object sample/CMakeFiles/learner.dir/net_utils.c.o"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/learner.dir/net_utils.c.o   -c /home/vagrant/libpaxos/sample/net_utils.c

sample/CMakeFiles/learner.dir/net_utils.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/learner.dir/net_utils.c.i"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/sample/net_utils.c > CMakeFiles/learner.dir/net_utils.c.i

sample/CMakeFiles/learner.dir/net_utils.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/learner.dir/net_utils.c.s"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/sample/net_utils.c -o CMakeFiles/learner.dir/net_utils.c.s

sample/CMakeFiles/learner.dir/net_utils.c.o.requires:
.PHONY : sample/CMakeFiles/learner.dir/net_utils.c.o.requires

sample/CMakeFiles/learner.dir/net_utils.c.o.provides: sample/CMakeFiles/learner.dir/net_utils.c.o.requires
	$(MAKE) -f sample/CMakeFiles/learner.dir/build.make sample/CMakeFiles/learner.dir/net_utils.c.o.provides.build
.PHONY : sample/CMakeFiles/learner.dir/net_utils.c.o.provides

sample/CMakeFiles/learner.dir/net_utils.c.o.provides.build: sample/CMakeFiles/learner.dir/net_utils.c.o

sample/CMakeFiles/learner.dir/leveldb_context.c.o: sample/CMakeFiles/learner.dir/flags.make
sample/CMakeFiles/learner.dir/leveldb_context.c.o: ../sample/leveldb_context.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object sample/CMakeFiles/learner.dir/leveldb_context.c.o"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/learner.dir/leveldb_context.c.o   -c /home/vagrant/libpaxos/sample/leveldb_context.c

sample/CMakeFiles/learner.dir/leveldb_context.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/learner.dir/leveldb_context.c.i"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/sample/leveldb_context.c > CMakeFiles/learner.dir/leveldb_context.c.i

sample/CMakeFiles/learner.dir/leveldb_context.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/learner.dir/leveldb_context.c.s"
	cd /home/vagrant/libpaxos/build/sample && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/sample/leveldb_context.c -o CMakeFiles/learner.dir/leveldb_context.c.s

sample/CMakeFiles/learner.dir/leveldb_context.c.o.requires:
.PHONY : sample/CMakeFiles/learner.dir/leveldb_context.c.o.requires

sample/CMakeFiles/learner.dir/leveldb_context.c.o.provides: sample/CMakeFiles/learner.dir/leveldb_context.c.o.requires
	$(MAKE) -f sample/CMakeFiles/learner.dir/build.make sample/CMakeFiles/learner.dir/leveldb_context.c.o.provides.build
.PHONY : sample/CMakeFiles/learner.dir/leveldb_context.c.o.provides

sample/CMakeFiles/learner.dir/leveldb_context.c.o.provides.build: sample/CMakeFiles/learner.dir/leveldb_context.c.o

# Object files for target learner
learner_OBJECTS = \
"CMakeFiles/learner.dir/learner.c.o" \
"CMakeFiles/learner.dir/application_config.c.o" \
"CMakeFiles/learner.dir/net_utils.c.o" \
"CMakeFiles/learner.dir/leveldb_context.c.o"

# External object files for target learner
learner_EXTERNAL_OBJECTS =

sample/learner: sample/CMakeFiles/learner.dir/learner.c.o
sample/learner: sample/CMakeFiles/learner.dir/application_config.c.o
sample/learner: sample/CMakeFiles/learner.dir/net_utils.c.o
sample/learner: sample/CMakeFiles/learner.dir/leveldb_context.c.o
sample/learner: sample/CMakeFiles/learner.dir/build.make
sample/learner: evpaxos/libevpaxos.so
sample/learner: paxos/libpaxos.a
sample/learner: /usr/lib/x86_64-linux-gnu/liblmdb.so
sample/learner: /usr/lib/x86_64-linux-gnu/libevent.so
sample/learner: /usr/local/lib/libmsgpackc.so
sample/learner: sample/CMakeFiles/learner.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable learner"
	cd /home/vagrant/libpaxos/build/sample && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/learner.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
sample/CMakeFiles/learner.dir/build: sample/learner
.PHONY : sample/CMakeFiles/learner.dir/build

sample/CMakeFiles/learner.dir/requires: sample/CMakeFiles/learner.dir/learner.c.o.requires
sample/CMakeFiles/learner.dir/requires: sample/CMakeFiles/learner.dir/application_config.c.o.requires
sample/CMakeFiles/learner.dir/requires: sample/CMakeFiles/learner.dir/net_utils.c.o.requires
sample/CMakeFiles/learner.dir/requires: sample/CMakeFiles/learner.dir/leveldb_context.c.o.requires
.PHONY : sample/CMakeFiles/learner.dir/requires

sample/CMakeFiles/learner.dir/clean:
	cd /home/vagrant/libpaxos/build/sample && $(CMAKE_COMMAND) -P CMakeFiles/learner.dir/cmake_clean.cmake
.PHONY : sample/CMakeFiles/learner.dir/clean

sample/CMakeFiles/learner.dir/depend:
	cd /home/vagrant/libpaxos/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/libpaxos /home/vagrant/libpaxos/sample /home/vagrant/libpaxos/build /home/vagrant/libpaxos/build/sample /home/vagrant/libpaxos/build/sample/CMakeFiles/learner.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : sample/CMakeFiles/learner.dir/depend

