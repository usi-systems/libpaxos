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
include caans/CMakeFiles/sequencer.dir/depend.make

# Include the progress variables for this target.
include caans/CMakeFiles/sequencer.dir/progress.make

# Include the compile flags for this target's objects.
include caans/CMakeFiles/sequencer.dir/flags.make

caans/CMakeFiles/sequencer.dir/sequencer.c.o: caans/CMakeFiles/sequencer.dir/flags.make
caans/CMakeFiles/sequencer.dir/sequencer.c.o: ../caans/sequencer.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sequencer.dir/sequencer.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sequencer.dir/sequencer.c.o   -c /home/vagrant/libpaxos/caans/sequencer.c

caans/CMakeFiles/sequencer.dir/sequencer.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sequencer.dir/sequencer.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/sequencer.c > CMakeFiles/sequencer.dir/sequencer.c.i

caans/CMakeFiles/sequencer.dir/sequencer.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sequencer.dir/sequencer.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/sequencer.c -o CMakeFiles/sequencer.dir/sequencer.c.s

caans/CMakeFiles/sequencer.dir/sequencer.c.o.requires:
.PHONY : caans/CMakeFiles/sequencer.dir/sequencer.c.o.requires

caans/CMakeFiles/sequencer.dir/sequencer.c.o.provides: caans/CMakeFiles/sequencer.dir/sequencer.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sequencer.dir/build.make caans/CMakeFiles/sequencer.dir/sequencer.c.o.provides.build
.PHONY : caans/CMakeFiles/sequencer.dir/sequencer.c.o.provides

caans/CMakeFiles/sequencer.dir/sequencer.c.o.provides.build: caans/CMakeFiles/sequencer.dir/sequencer.c.o

caans/CMakeFiles/sequencer.dir/configuration.c.o: caans/CMakeFiles/sequencer.dir/flags.make
caans/CMakeFiles/sequencer.dir/configuration.c.o: ../caans/configuration.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sequencer.dir/configuration.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sequencer.dir/configuration.c.o   -c /home/vagrant/libpaxos/caans/configuration.c

caans/CMakeFiles/sequencer.dir/configuration.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sequencer.dir/configuration.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/configuration.c > CMakeFiles/sequencer.dir/configuration.c.i

caans/CMakeFiles/sequencer.dir/configuration.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sequencer.dir/configuration.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/configuration.c -o CMakeFiles/sequencer.dir/configuration.c.s

caans/CMakeFiles/sequencer.dir/configuration.c.o.requires:
.PHONY : caans/CMakeFiles/sequencer.dir/configuration.c.o.requires

caans/CMakeFiles/sequencer.dir/configuration.c.o.provides: caans/CMakeFiles/sequencer.dir/configuration.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sequencer.dir/build.make caans/CMakeFiles/sequencer.dir/configuration.c.o.provides.build
.PHONY : caans/CMakeFiles/sequencer.dir/configuration.c.o.provides

caans/CMakeFiles/sequencer.dir/configuration.c.o.provides.build: caans/CMakeFiles/sequencer.dir/configuration.c.o

caans/CMakeFiles/sequencer.dir/application_proxy.c.o: caans/CMakeFiles/sequencer.dir/flags.make
caans/CMakeFiles/sequencer.dir/application_proxy.c.o: ../caans/application_proxy.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sequencer.dir/application_proxy.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sequencer.dir/application_proxy.c.o   -c /home/vagrant/libpaxos/caans/application_proxy.c

caans/CMakeFiles/sequencer.dir/application_proxy.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sequencer.dir/application_proxy.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/application_proxy.c > CMakeFiles/sequencer.dir/application_proxy.c.i

caans/CMakeFiles/sequencer.dir/application_proxy.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sequencer.dir/application_proxy.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/application_proxy.c -o CMakeFiles/sequencer.dir/application_proxy.c.s

caans/CMakeFiles/sequencer.dir/application_proxy.c.o.requires:
.PHONY : caans/CMakeFiles/sequencer.dir/application_proxy.c.o.requires

caans/CMakeFiles/sequencer.dir/application_proxy.c.o.provides: caans/CMakeFiles/sequencer.dir/application_proxy.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sequencer.dir/build.make caans/CMakeFiles/sequencer.dir/application_proxy.c.o.provides.build
.PHONY : caans/CMakeFiles/sequencer.dir/application_proxy.c.o.provides

caans/CMakeFiles/sequencer.dir/application_proxy.c.o.provides.build: caans/CMakeFiles/sequencer.dir/application_proxy.c.o

caans/CMakeFiles/sequencer.dir/message.c.o: caans/CMakeFiles/sequencer.dir/flags.make
caans/CMakeFiles/sequencer.dir/message.c.o: ../caans/message.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sequencer.dir/message.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sequencer.dir/message.c.o   -c /home/vagrant/libpaxos/caans/message.c

caans/CMakeFiles/sequencer.dir/message.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sequencer.dir/message.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/message.c > CMakeFiles/sequencer.dir/message.c.i

caans/CMakeFiles/sequencer.dir/message.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sequencer.dir/message.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/message.c -o CMakeFiles/sequencer.dir/message.c.s

caans/CMakeFiles/sequencer.dir/message.c.o.requires:
.PHONY : caans/CMakeFiles/sequencer.dir/message.c.o.requires

caans/CMakeFiles/sequencer.dir/message.c.o.provides: caans/CMakeFiles/sequencer.dir/message.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sequencer.dir/build.make caans/CMakeFiles/sequencer.dir/message.c.o.provides.build
.PHONY : caans/CMakeFiles/sequencer.dir/message.c.o.provides

caans/CMakeFiles/sequencer.dir/message.c.o.provides.build: caans/CMakeFiles/sequencer.dir/message.c.o

# Object files for target sequencer
sequencer_OBJECTS = \
"CMakeFiles/sequencer.dir/sequencer.c.o" \
"CMakeFiles/sequencer.dir/configuration.c.o" \
"CMakeFiles/sequencer.dir/application_proxy.c.o" \
"CMakeFiles/sequencer.dir/message.c.o"

# External object files for target sequencer
sequencer_EXTERNAL_OBJECTS =

caans/sequencer: caans/CMakeFiles/sequencer.dir/sequencer.c.o
caans/sequencer: caans/CMakeFiles/sequencer.dir/configuration.c.o
caans/sequencer: caans/CMakeFiles/sequencer.dir/application_proxy.c.o
caans/sequencer: caans/CMakeFiles/sequencer.dir/message.c.o
caans/sequencer: caans/CMakeFiles/sequencer.dir/build.make
caans/sequencer: net/libnetpaxos.so
caans/sequencer: /usr/local/lib/libleveldb.so
caans/sequencer: paxos/libpaxos.a
caans/sequencer: /usr/lib/x86_64-linux-gnu/liblmdb.so
caans/sequencer: /usr/lib/x86_64-linux-gnu/libevent.so
caans/sequencer: caans/CMakeFiles/sequencer.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable sequencer"
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/sequencer.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
caans/CMakeFiles/sequencer.dir/build: caans/sequencer
.PHONY : caans/CMakeFiles/sequencer.dir/build

caans/CMakeFiles/sequencer.dir/requires: caans/CMakeFiles/sequencer.dir/sequencer.c.o.requires
caans/CMakeFiles/sequencer.dir/requires: caans/CMakeFiles/sequencer.dir/configuration.c.o.requires
caans/CMakeFiles/sequencer.dir/requires: caans/CMakeFiles/sequencer.dir/application_proxy.c.o.requires
caans/CMakeFiles/sequencer.dir/requires: caans/CMakeFiles/sequencer.dir/message.c.o.requires
.PHONY : caans/CMakeFiles/sequencer.dir/requires

caans/CMakeFiles/sequencer.dir/clean:
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -P CMakeFiles/sequencer.dir/cmake_clean.cmake
.PHONY : caans/CMakeFiles/sequencer.dir/clean

caans/CMakeFiles/sequencer.dir/depend:
	cd /home/vagrant/libpaxos/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/libpaxos /home/vagrant/libpaxos/caans /home/vagrant/libpaxos/build /home/vagrant/libpaxos/build/caans /home/vagrant/libpaxos/build/caans/CMakeFiles/sequencer.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : caans/CMakeFiles/sequencer.dir/depend

