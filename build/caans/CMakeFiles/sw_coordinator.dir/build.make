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
include caans/CMakeFiles/sw_coordinator.dir/depend.make

# Include the progress variables for this target.
include caans/CMakeFiles/sw_coordinator.dir/progress.make

# Include the compile flags for this target's objects.
include caans/CMakeFiles/sw_coordinator.dir/flags.make

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o: caans/CMakeFiles/sw_coordinator.dir/flags.make
caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o: ../caans/coordinator.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sw_coordinator.dir/coordinator.c.o   -c /home/vagrant/libpaxos/caans/coordinator.c

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sw_coordinator.dir/coordinator.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/coordinator.c > CMakeFiles/sw_coordinator.dir/coordinator.c.i

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sw_coordinator.dir/coordinator.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/coordinator.c -o CMakeFiles/sw_coordinator.dir/coordinator.c.s

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.requires:
.PHONY : caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.requires

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.provides: caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sw_coordinator.dir/build.make caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.provides.build
.PHONY : caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.provides

caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.provides.build: caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o

caans/CMakeFiles/sw_coordinator.dir/configuration.c.o: caans/CMakeFiles/sw_coordinator.dir/flags.make
caans/CMakeFiles/sw_coordinator.dir/configuration.c.o: ../caans/configuration.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sw_coordinator.dir/configuration.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sw_coordinator.dir/configuration.c.o   -c /home/vagrant/libpaxos/caans/configuration.c

caans/CMakeFiles/sw_coordinator.dir/configuration.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sw_coordinator.dir/configuration.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/configuration.c > CMakeFiles/sw_coordinator.dir/configuration.c.i

caans/CMakeFiles/sw_coordinator.dir/configuration.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sw_coordinator.dir/configuration.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/configuration.c -o CMakeFiles/sw_coordinator.dir/configuration.c.s

caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.requires:
.PHONY : caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.requires

caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.provides: caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sw_coordinator.dir/build.make caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.provides.build
.PHONY : caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.provides

caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.provides.build: caans/CMakeFiles/sw_coordinator.dir/configuration.c.o

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o: caans/CMakeFiles/sw_coordinator.dir/flags.make
caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o: ../caans/application_proxy.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sw_coordinator.dir/application_proxy.c.o   -c /home/vagrant/libpaxos/caans/application_proxy.c

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sw_coordinator.dir/application_proxy.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/application_proxy.c > CMakeFiles/sw_coordinator.dir/application_proxy.c.i

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sw_coordinator.dir/application_proxy.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/application_proxy.c -o CMakeFiles/sw_coordinator.dir/application_proxy.c.s

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.requires:
.PHONY : caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.requires

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.provides: caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sw_coordinator.dir/build.make caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.provides.build
.PHONY : caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.provides

caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.provides.build: caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o

caans/CMakeFiles/sw_coordinator.dir/message.c.o: caans/CMakeFiles/sw_coordinator.dir/flags.make
caans/CMakeFiles/sw_coordinator.dir/message.c.o: ../caans/message.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/vagrant/libpaxos/build/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object caans/CMakeFiles/sw_coordinator.dir/message.c.o"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/sw_coordinator.dir/message.c.o   -c /home/vagrant/libpaxos/caans/message.c

caans/CMakeFiles/sw_coordinator.dir/message.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/sw_coordinator.dir/message.c.i"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/vagrant/libpaxos/caans/message.c > CMakeFiles/sw_coordinator.dir/message.c.i

caans/CMakeFiles/sw_coordinator.dir/message.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/sw_coordinator.dir/message.c.s"
	cd /home/vagrant/libpaxos/build/caans && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/vagrant/libpaxos/caans/message.c -o CMakeFiles/sw_coordinator.dir/message.c.s

caans/CMakeFiles/sw_coordinator.dir/message.c.o.requires:
.PHONY : caans/CMakeFiles/sw_coordinator.dir/message.c.o.requires

caans/CMakeFiles/sw_coordinator.dir/message.c.o.provides: caans/CMakeFiles/sw_coordinator.dir/message.c.o.requires
	$(MAKE) -f caans/CMakeFiles/sw_coordinator.dir/build.make caans/CMakeFiles/sw_coordinator.dir/message.c.o.provides.build
.PHONY : caans/CMakeFiles/sw_coordinator.dir/message.c.o.provides

caans/CMakeFiles/sw_coordinator.dir/message.c.o.provides.build: caans/CMakeFiles/sw_coordinator.dir/message.c.o

# Object files for target sw_coordinator
sw_coordinator_OBJECTS = \
"CMakeFiles/sw_coordinator.dir/coordinator.c.o" \
"CMakeFiles/sw_coordinator.dir/configuration.c.o" \
"CMakeFiles/sw_coordinator.dir/application_proxy.c.o" \
"CMakeFiles/sw_coordinator.dir/message.c.o"

# External object files for target sw_coordinator
sw_coordinator_EXTERNAL_OBJECTS =

caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o
caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/configuration.c.o
caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o
caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/message.c.o
caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/build.make
caans/sw_coordinator: net/libnetpaxos.so
caans/sw_coordinator: /usr/local/lib/libleveldb.so
caans/sw_coordinator: paxos/libpaxos.a
caans/sw_coordinator: /usr/lib/x86_64-linux-gnu/liblmdb.so
caans/sw_coordinator: /usr/lib/x86_64-linux-gnu/libevent.so
caans/sw_coordinator: caans/CMakeFiles/sw_coordinator.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable sw_coordinator"
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/sw_coordinator.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
caans/CMakeFiles/sw_coordinator.dir/build: caans/sw_coordinator
.PHONY : caans/CMakeFiles/sw_coordinator.dir/build

caans/CMakeFiles/sw_coordinator.dir/requires: caans/CMakeFiles/sw_coordinator.dir/coordinator.c.o.requires
caans/CMakeFiles/sw_coordinator.dir/requires: caans/CMakeFiles/sw_coordinator.dir/configuration.c.o.requires
caans/CMakeFiles/sw_coordinator.dir/requires: caans/CMakeFiles/sw_coordinator.dir/application_proxy.c.o.requires
caans/CMakeFiles/sw_coordinator.dir/requires: caans/CMakeFiles/sw_coordinator.dir/message.c.o.requires
.PHONY : caans/CMakeFiles/sw_coordinator.dir/requires

caans/CMakeFiles/sw_coordinator.dir/clean:
	cd /home/vagrant/libpaxos/build/caans && $(CMAKE_COMMAND) -P CMakeFiles/sw_coordinator.dir/cmake_clean.cmake
.PHONY : caans/CMakeFiles/sw_coordinator.dir/clean

caans/CMakeFiles/sw_coordinator.dir/depend:
	cd /home/vagrant/libpaxos/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/libpaxos /home/vagrant/libpaxos/caans /home/vagrant/libpaxos/build /home/vagrant/libpaxos/build/caans /home/vagrant/libpaxos/build/caans/CMakeFiles/sw_coordinator.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : caans/CMakeFiles/sw_coordinator.dir/depend

