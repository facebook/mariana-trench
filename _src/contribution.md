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
$ brew install python3 cmake zlib boost googletest jsoncpp re2
```

On **Linux**, run:
```shell
$ brew install cmake zlib boost jsoncpp re2
$ brew install googletest --build-from-source # The package is currently broken.
$ export CMAKE_PREFIX_PATH=/home/linuxbrew/.linuxbrew/opt/jsoncpp:/home/linuxbrew/.linuxbrew/opt/zlib
```

On **Linux**, you will need to install Java to run the tests. For instance, on **Ubuntu**, run:
```shell
$ sudo apt install default-jre default-jdk
```

### Building fmt

The 9.0 release of `fmt` has breaking changes that Mariana Trench is not yet compatible with, so for now, you need to build the library from source. You will need to do the following:

```shell
$ git clone https://github.com/fmtlib/fmt.git
$ git checkout 8.1.1
$ mkdir fmt/build
$ cd fmt/build
$ cmake ..
$ make
$ make install
```

### Building Redex

You will need to choose a temporary directory to store the C++ binaries and libraries for Redex and Mariana Trench. You can safely remove these after installation if you do not intend to update the C++ code. Pick a directory and set the variable `MT_INSTALL_DIRECTORY` to its absolute path, for instance:
```shell
$ MT_INSTALL_DIRECTORY="$PWD/install"
```

To build [Redex](https://fbredex.com/) from source, run:
```shell
$ brew install git
$ git clone https://github.com/facebook/redex.git
$ cd redex
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX="$MT_INSTALL_DIRECTORY" ..
$ make -j4
$ make install
```

### Building Mariana Trench

Now that we have our dependencies ready, let's build the Mariana Trench binary:
```shell
$ cd ../..  # Go back to the root directory
$ mkdir build
$ cd build
$ cmake -DREDEX_ROOT="$MT_INSTALL_DIRECTORY" -DCMAKE_INSTALL_PREFIX="$MT_INSTALL_DIRECTORY" ..
$ make -j4
$ make install
```

Finally, let's install Mariana Trench as a Python package.
We recommend to run this step inside a [virtual environment](https://packaging.python.org/tutorials/installing-packages/#creating-virtual-environments).
```shell
$ cd .. # Go back to the root directory
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
