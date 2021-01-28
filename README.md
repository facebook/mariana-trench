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
* Redex (master)

### macOS

Here are the steps to install Mariana Trench on **macOS** using **[Homebrew](https://brew.sh/)**.

You will need to choose an installation directory that will contain all the binaries, libraries and headers after installation. If you do not specify it, everything will be installed under `/usr/local`.

You will need Xcode with command line tools installed. To get the command line tools, use:

```
xcode-select --install
```

Then, install all the required dependencies with [Homebrew](https://brew.sh/):

```
brew upgrade
brew install python3 cmake zlib boost googletest jsoncpp fmt re2
```

Now, you will need to build [Redex](https://fbredex.com/) from source:
```
brew install git
git clone https://github.com/facebook/redex.git
cd redex
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX="<install-directory>" ..
make -j4
make install
```

Finally, you can build and install Mariana Trench:
```
cd ../..  # Go back to the root directory
mkdir build
cd build
cmake -DREDEX_ROOT="<install-directory>" -DCMAKE_INSTALL_PREFIX="<install-directory>" ..
make -j4
make install
```
