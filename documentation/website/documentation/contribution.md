---
id: contribution
title: Contribution
sidebar_label: Contribution
---

This documentation aims to help you become an active contributor to Mariana Trench.

## Building From Source

### Support

Mariana Trench is currently supported on **macOS** (tested on *Big Sur 11.4*) and **Linux** (tested on *Ubuntu 20.04 LTS*).

### Dependencies

Below is a list of the required dependencies. Most of them can be installed with **[Homebrew](https://brew.sh/)**.

* A C++ compiler that supports C++17 (GCC >= 7 or Clang >= 5)
* Python >= 3.6
* CMake >= 3.19.3
* zlib
* Boost >= 1.75.0
* GoogleTest >= 1.10.0
* JsonCpp >= 1.9.4
* fmt >= 7.1.2, <= 8.1.1
* RE2
* Java (Optional)
* Android SDK (Optional)
* Redex (master)

### Install all dependencies with Homebrew

First, follow the instructions to install **[Homebrew](https://brew.sh/)** on your system.

Then, make sure homebrew is up-to-date:
```shell
$ brew update
$ brew upgrade
```

Finally, install all the dependencies.

On **macOS**, run:
```shell
$ brew install python3 git cmake zlib boost googletest jsoncpp re2
```

On **Linux**, run:
```shell
$ brew install git cmake zlib boost jsoncpp re2
$ brew install googletest --build-from-source # The package is currently broken.
$ export CMAKE_PREFIX_PATH=/home/linuxbrew/.linuxbrew/opt/jsoncpp:/home/linuxbrew/.linuxbrew/opt/zlib
```

On **Linux**, you will need to install Java to run the tests. For instance, on **Ubuntu**, run:
```shell
$ sudo apt install default-jre default-jdk
```

### Clone the repository

First of, clone the Mariana Trench repository. We will also set an environment variable `MARIANA_TRENCH_DIRECTORY` that points to it for the following instructions.
```shell
$ git clone https://github.com/facebook/mariana-trench.git
$ cd mariana-trench
$ MARIANA_TRENCH_DIRECTORY="$PWD"
```

### Installation directory

We do not recommend installing Mariana Trench as root. Instead, we will install all libraries and binaries in a directory "install".
We will also use a directory called "dependencies" to store dependencies that we have to build from source.
Run the following commands:
```shell
$ mkdir install
$ mkdir dependencies
```

### Building fmt

The 9.0 release of `fmt` has breaking changes that Mariana Trench is not yet compatible with, so for now, you need to build the library from source. You will need to do the following:

```shell
$ cd "$MARIANA_TRENCH_DIRECTORY/dependencies"
$ git clone -b 8.1.1 https://github.com/fmtlib/fmt.git
$ mkdir fmt/build
$ cd fmt/build
$ cmake -DCMAKE_INSTALL_PREFIX="$MARIANA_TRENCH_DIRECTORY/install" ..
$ make -j4
$ make install
```

### Building Redex

We also need to build [Redex](https://fbredex.com/) from source, run:
```shell
$ cd "$MARIANA_TRENCH_DIRECTORY/dependencies"
$ git clone https://github.com/facebook/redex.git
$ mkdir redex/build
$ cd redex/build
$ cmake -DCMAKE_INSTALL_PREFIX="$MARIANA_TRENCH_DIRECTORY/install" ..
$ make -j4
$ make install
```

### Building Mariana Trench

Now that we have our dependencies ready, let's build the Mariana Trench binary:
```shell
$ cd "$MARIANA_TRENCH_DIRECTORY"
$ mkdir build
$ cd build
$ cmake \
  -DREDEX_ROOT="$MARIANA_TRENCH_DIRECTORY/install" \
  -Dfmt_ROOT="$MARIANA_TRENCH_DIRECTORY/install" \
  -DCMAKE_INSTALL_PREFIX="$MARIANA_TRENCH_DIRECTORY/install" \
  ..
$ make -j4
$ make install
```

Finally, let's install Mariana Trench as a Python package.
First, follow the instructions to create a [virtual environment](https://packaging.python.org/tutorials/installing-packages/#creating-virtual-environments).
Once inside a virtual environment (after using the `activate` script), run:
```shell
$ cd "$MARIANA_TRENCH_DIRECTORY"
$ python scripts/setup.py \
  --binary "$MT_INSTALL_DIRECTORY/bin/mariana-trench-binary" \
  --pyredex "$MT_INSTALL_DIRECTORY/bin/pyredex" \
  install
```

## Development

If you are making changes to Mariana Trench, you can use the `mariana-trench` wrapper inside the build directory:
```shell
$ cd build
$ ./mariana-trench --help
```

This way, you don't have to call `scripts/setup.py` between every changes.
Python changes will be automatically picked up.
C++ changes will be picked up after running `make`.

Note that you will need to install all python dependencies:
```shell
$ pip install pyre_extensions fb-sapp
```

## Run the tests

To run the tests after building Mariana Trench, use:
```shell
$ cd build
$ make check
```

## Troubleshooting

Here are a set of errors you might encounter, and their solutions.

### CMake Warning: Ignoring extra path from command line: ".."

You probably tried to run `cmake` from the wrong directory.
Make sure that `$MARIANA_TRENCH_DIRECTORY` is set correctly (test with `echo $MARIANA_TRENCH_DIRECTORY`).
Then, run the instructions again from the beginning of the section you are in.

### error: externally-managed-environment

You probably tried to run `python scripts/setup.py` without a virtual environment. Create a [virtual environment](https://packaging.python.org/tutorials/installing-packages/#creating-virtual-environments) first.

### undefined reference to `pthread_create@GLIBC`

This seems to happen on Linux, when your operating system has an old version of glibc, which doesn't match the version used by Homebrew.
Try upgrading your operating system to the last version.

Another option is to use the compiler (gcc) from Homebrew directly:
```
$ brew install gcc
export CC=/home/linuxbrew/.linuxbrew/bin/cc
export CXX=/home/linuxbrew/.linuxbrew/bin/c++
```
You will need to run all the instructions from this page again, starting from `Clone the repository`. We recommend starting from scratch, i.e delete the mariana-trench directory.
