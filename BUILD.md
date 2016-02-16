## Build instructions

A "premade" version of these instructions can be found in the file
build.sh, and there is also a "clean up" script called clean.sh (that
removes all of the files that are checked out or made)

You can get the build.sh by (suggest that you create a directory to do
this in):

     wget https://raw.githubusercontent.com/Leporacanthicus/lacsap/master/build.sh

and clean using:

     wget https://raw.githubusercontent.com/Leporacanthicus/lacsap/master/clean.sh

You will need to `chmod +x build.sh` since wget doesn't allow the
automatically mark the file as executable.

This file tries to describe the minimal steps required to build the
`lacsap` Pascal compiler. In many cases, "you may also want to ..."
examples are provided to give suggestions of things that may help your
development process, even if it's not necessary. 

### Prerequisites

You will need the development tools to build llvm. That includes
`cmake` build system, `git` version control, `gcc` or `clang` with
C++11 support, including the C and C++ standard libraries as well as
`libdl`	(dynamic library handling), `libz` (compression), `libtinfo`
(terminal information, used to determine if colours are supported) and
`libpthread` (threading library).

### Fetch the latest lacsap compiler

Using git, we'd do, in some suitable directory: 

     cd where-you-want-sources-to-go
     git clone https://github.com/Leporacanthicus/lacsap.git

This will create a directory called `lacsap` in the directory you `cd`
to in the first line. 


### Building `llvm`

A detailed description, by the llvm-project of how to build llvm can
be found [here][1].

Since lacsap uses git, it is my idea to use the same version control
system to track the `llvm` version used by this project.

The lacsap project provides a simple example file to clone the git
repository and select (in git parlance `git checkout`). It is called
`llvmversion` - it is not an executable file, but `source` in the
shell will perform the commands in it:

     cd where-you-want-sources-to-go
     source lacsap/llvmversion

This will download the llvm repository sources and create a `llvm`
directory.

Hint: If you want to keep multiple versions of llvm around, or some
such, then you may want to copy `llvmversion` and update the name
after `git clone` to something other than `llvm`.


Now, you need to run `cmake` in a new directory to build `llvm`:

     mkdir build-llvm 
     cd build-llvm
     cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local/llvm -DLLVM_TARGETS_TO_BUILD=X86 ../llvm

Note that the `cmake` command above describes where `llvm` will be
installed - in this case `/usr/local/llvm`. You can change this - it
may be necessary to do so if you do not have root access on the
machine, as `/usr` is typically not writeable for "ordinary
users". The actual location isn't particularly important.


Now cmake should have created a `Makefile` that builds the llvm
project. Now it's time to run `make`:

     make -j 6

where the 6 is the number of parallel compilations to run, which
should be approximately 30-50% higher than the number of processor
cores of the build system. So `-j 6` on a quad-core machine. It's
approximately, since the actual number also affects memory
usage. `make -j 8` works fine currently on a machine with 16GB of RAM
and fairly light load otherwise. Too large a value will cause
swapping, and actually slow down the process. 

This will take a little while. On a decent desktop machine compiling a
debug build of llvm takes 10-20 minutes, so now is probably a good
time to go get a cup of coffee, walk the dog, feed the fish, browse
the web, [fight with swords on office chairs][2] or all of the above.

Hopefully nothing goes wrong with the compilation - if it does, it's
probably because some pre-requisite component isn't installed. Check
the link to the LLVM build instructions at the beginning of this
section.

*Optional*: Check that the llvm build is "good". This is a good idea,
to make sure that you don't have a bad version of llvm itself.

    make check-llvm

This will build a few extra components that are needed for testing
only, and then run several thousand test-cases. It should all pass. If
it doesn't, it's hard to say here what is the right thing to
do. Trying another version is probably the first step.

Now you need to install the llvm into the "install prefix" directory:

    make install 

(You may need to do this with `sudo` if the install directory is
something like `/usr/...`)

Finally, go back up to where your main sources directory is:

    cd ..

### Building lacsap.

Now we're really at the home stretch, and only the Pascal compiler
itself left to build.

Go to the directory where you have your sources (from above 
`cd where-you-want-sources-to-go`) then
   
     cd lacsap

and finally, using `gcc` to build the Pascal compiler:

     make USECLANG=0

Extra options: 
    
     M32={1,0}   
         Enable or disable 32-bit build option.

     LLVM_DIR={where-llvm-install}
         The directory used to install LLVM into.

Optionally, you could build with my favoured C++ compiler `clang` instead:

     make 


[1] http://llvm.org/docs/CMake.html
[2] https://xkcd.com/303/
