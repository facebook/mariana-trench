# Mariana Trench

![logo](https://github.com/facebookincubator/mariana-trench/blob/master/logo.png?raw=true)

[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat)](http://choosealicense.com/licenses/mit/)

Mariana Trench is a security focused static analysis platform targeting Android.

## License

Mariana Trench is licensed under the MIT license.

## Installation

### Dependencies

Below is a list of the required dependencies. Most of them can be installed with a package manager.

* A C++ compiler that supports C++17 (GCC >= 7 or Clang >= 5)
* Python >= 3.6
* CMake >= 3.19.3
* zlib
* Boost >= 1.75.0
* GoogleTest >= 1.10.0
* JsonCpp >= 1.9.4
* fmt >= 7.1.2
* RE2
* Java (Optional)
* Android SDK (Optional)
* Redex (master)

### macOS

Here are the steps to install Mariana Trench on **macOS** using **[Homebrew](https://brew.sh/)**.

First, you will need to choose a temporary directory to store the C++ binaries and libraries. You can safely remove these after installation if you do not intend to update the C++ code. We will refer to this temporary directory as `<temporary-directory>` in the following instructions.

You will need Xcode with command line tools installed. To get the command line tools, use:

```shell
xcode-select --install
```

Then, install all the required dependencies with [Homebrew](https://brew.sh/):

```shell
brew upgrade
brew install python3 cmake zlib boost googletest jsoncpp fmt re2
```

Now, let's build [Redex](https://fbredex.com/) from source:
```shell
brew install git
git clone https://github.com/facebook/redex.git
cd redex
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX="<temporary-directory>" ..
make -j4
make install
```

Now, let's build the Mariana Trench binary:
```shell
cd ../..  # Go back to the root directory
mkdir build
cd build
cmake -DREDEX_ROOT="<temporary-directory>" -DCMAKE_INSTALL_PREFIX="<temporary-directory>" ..
make -j4
make install
```

Finally, let's install Mariana Trench as a Python package.
We recommend to run this step inside a [virtual environment](https://packaging.python.org/tutorials/installing-packages/#creating-virtual-environments).
```shell
cd .. # Go back to the root directory
python scripts/setup.py \
  --binary "<temporary-directory>/bin/mariana-trench-binary" \
  --pyredex "<temporary-directory>/bin/pyredex" \
  install
```

### Development

If you are making changes to Mariana Trench, you can use the `mariana-trench` wrapper inside the build directory:
```shell
cd build
./mariana-trench -h
```

This way, you don't have to use `scripts/setup.py` between every changes.
Python changes will be automatically picked up.
C++ changes will be picked up after running `make`.

Note that you will need to install all python dependencies:
```shell
pip install pyre_extensions fb-sapp
```

### Run the tests

To run the tests after building Mariana Trench, use:
```shell
cd build
make check
```
