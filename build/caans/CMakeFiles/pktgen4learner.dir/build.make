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
include caans/CMakeFiles/pktgen4learner.dir/depend.make

# Include the progress variables for this target.
include caans/CMakeFiles/pktgen4learner.dir/progress.make

# Include the compile flags for this target's objects.
include caans/CMakeFiles/pktgen4learner.dir/flags.make

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o: caans/CMakeFiles/pktgen4learner.dir/flags.make
caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o: ../caans/pktgen4learner.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o   -c /home/vagrant/libpaxos/caans/pktgen4learner.c

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/pktgen4learner.dir/pktgen4learner.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/pktgen4learner.c > CMakeFiles/pktgen4learner.dir/pktgen4learner.c.i

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/pktgen4learner.dir/pktgen4learner.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/pktgen4learner.c -o CMakeFiles/pktgen4learner.dir/pktgen4learner.c.s

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.requires:
.PHONY : caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.requires

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.provides: caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.requires
	$(MAKE) -f caans/CMakeFiles/pktgen4learner.dir/build.make caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.provides.build
.PHONY : caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.provides

caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.provides.build: caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o

caans/CMakeFiles/pktgen4learner.dir/configuration.c.o: caans/CMakeFiles/pktgen4learner.dir/flags.make
caans/CMakeFiles/pktgen4learner.dir/configuration.c.o: ../caans/configuration.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/pktgen4learner.dir/configuration.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/pktgen4learner.dir/configuration.c.o   -c /home/vagrant/libpaxos/caans/configuration.c

caans/CMakeFiles/pktgen4learner.dir/configuration.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/pktgen4learner.dir/configuration.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/configuration.c > CMakeFiles/pktgen4learner.dir/configuration.c.i

caans/CMakeFiles/pktgen4learner.dir/configuration.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/pktgen4learner.dir/configuration.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/configuration.c -o CMakeFiles/pktgen4learner.dir/configuration.c.s

caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.requires:
.PHONY : caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.requires

caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.provides: caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.requires
	$(MAKE) -f caans/CMakeFiles/pktgen4learner.dir/build.make caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.provides.build
.PHONY : caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.provides

caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.provides.build: caans/CMakeFiles/pktgen4learner.dir/configuration.c.o

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o: caans/CMakeFiles/pktgen4learner.dir/flags.make
caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o: ../caans/application_proxy.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/pktgen4learner.dir/application_proxy.c.o   -c /home/vagrant/libpaxos/caans/application_proxy.c

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/pktgen4learner.dir/application_proxy.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/application_proxy.c > CMakeFiles/pktgen4learner.dir/application_proxy.c.i

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/pktgen4learner.dir/application_proxy.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/application_proxy.c -o CMakeFiles/pktgen4learner.dir/application_proxy.c.s

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.requires:
.PHONY : caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.requires

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.provides: caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.requires
	$(MAKE) -f caans/CMakeFiles/pktgen4learner.dir/build.make caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.provides.build
.PHONY : caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.provides

caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.provides.build: caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o

caans/CMakeFiles/pktgen4learner.dir/message.c.o: caans/CMakeFiles/pktgen4learner.dir/flags.make
caans/CMakeFiles/pktgen4learner.dir/message.c.o: ../caans/message.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/pktgen4learner.dir/message.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/pktgen4learner.dir/message.c.o   -c /home/vagrant/libpaxos/caans/message.c

caans/CMakeFiles/pktgen4learner.dir/message.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/pktgen4learner.dir/message.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/message.c > CMakeFiles/pktgen4learner.dir/message.c.i

caans/CMakeFiles/pktgen4learner.dir/message.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/pktgen4learner.dir/message.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/message.c -o CMakeFiles/pktgen4learner.dir/message.c.s

caans/CMakeFiles/pktgen4learner.dir/message.c.o.requires:
.PHONY : caans/CMakeFiles/pktgen4learner.dir/message.c.o.requires

caans/CMakeFiles/pktgen4learner.dir/message.c.o.provides: caans/CMakeFiles/pktgen4learner.dir/message.c.o.requires
	$(MAKE) -f caans/CMakeFiles/pktgen4learner.dir/build.make caans/CMakeFiles/pktgen4learner.dir/message.c.o.provides.build
.PHONY : caans/CMakeFiles/pktgen4learner.dir/message.c.o.provides

caans/CMakeFiles/pktgen4learner.dir/message.c.o.provides.build: caans/CMakeFiles/pktgen4learner.dir/message.c.o

# Object files for target pktgen4learner
pktgen4learner_OBJECTS = \
"CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o" \
"CMakeFiles/pktgen4learner.dir/configuration.c.o" \
"CMakeFiles/pktgen4learner.dir/application_proxy.c.o" \
"CMakeFiles/pktgen4learner.dir/message.c.o"

# External object files for target pktgen4learner
pktgen4learner_EXTERNAL_OBJECTS =

caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o
caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/configuration.c.o
caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o
caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/message.c.o
caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/build.make
caans/pktgen4learner: net/libnetpaxos.so
caans/pktgen4learner: /usr/local/lib/libleveldb.so
caans/pktgen4learner: paxos/libpaxos.a
caans/pktgen4learner: /usr/lib/x86_64-linux-gnu/liblmdb.so
caans/pktgen4learner: /usr/lib/x86_64-linux-gnu/libevent.so
caans/pktgen4learner: caans/CMakeFiles/pktgen4learner.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable pktgen4learner"
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/pktgen4learner.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
caans/CMakeFiles/pktgen4learner.dir/build: caans/pktgen4learner
.PHONY : caans/CMakeFiles/pktgen4learner.dir/build

caans/CMakeFiles/pktgen4learner.dir/requires: caans/CMakeFiles/pktgen4learner.dir/pktgen4learner.c.o.requires
caans/CMakeFiles/pktgen4learner.dir/requires: caans/CMakeFiles/pktgen4learner.dir/configuration.c.o.requires
caans/CMakeFiles/pktgen4learner.dir/requires: caans/CMakeFiles/pktgen4learner.dir/application_proxy.c.o.requires
caans/CMakeFiles/pktgen4learner.dir/requires: caans/CMakeFiles/pktgen4learner.dir/message.c.o.requires
.PHONY : caans/CMakeFiles/pktgen4learner.dir/requires

caans/CMakeFiles/pktgen4learner.dir/clean:
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -P CMakeFiles/pktgen4learner.dir/cmake_clean.cmake
.PHONY : caans/CMakeFiles/pktgen4learner.dir/clean

caans/CMakeFiles/pktgen4learner.dir/depend:
	cd /home/vagrant/libpaxos/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/libpaxos /home/vagrant/libpaxos/caans /home/vagrant/libpaxos/build /home/vagrant/libpaxos/build/caans /home/vagrant/libpaxos/build/caans/CMakeFiles/pktgen4learner.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : caans/CMakeFiles/pktgen4learner.dir/depend

