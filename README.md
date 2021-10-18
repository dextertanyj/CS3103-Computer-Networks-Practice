# README

## Requirements
The HTTPS proxy has been tested to compile and run on both XCNE 1 and 2, however listed below are the requirements should you want to run the proxy on another machine.
- Ubuntu (Recommended: LTS 20.04)
- GCC version 9.3.0 or later (Default version shipped with Ubuntu 20.04)
- Boost C++ Libraries (Available at: https://www.boost.org)

## Usage
1. Download the archive.
1. Uncompress the archive using `$ tar -zxvf <archive_name>`
1. The above command should setup the files in the following structure:
    ```
    proxy/
    ├- README.md
    ├- makefile
    └- src/
        ├- main.cpp
        ├- server.hpp
        ├- server.cpp
        ├- ...
        └- logger/
           └- ...
    ```
1. To compile the proxy, you can use the makefile provided in the `proxy` folder: `$ make`
    - If you need to clean up the object files and executable, you can use: `$ make clean`
1. Start the proxy using `./proxy PORT [TELEMETRY_FLAG [PATH_TO_BLACKLIST]]`
