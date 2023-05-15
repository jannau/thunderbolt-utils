# tbtutils
Thunderbolt/USB4 utilities

This software is a collection of various user-space functionalities for the thunderbolt/USB4 subsystem, along with a library (lstbt) to query the TBT/USB4 devices present in
the system. 

## Building and installing lstbt
lstbt is present in the ./lib subdirectory of the software.

Steps:<br>
1. `cd ./lib`
2. `make`

## Uninstalling lstbt and cleaning up the build

Steps:<br>
1. `cd ./lib`
2. `make clean`

Note: The library installs itself in the /usr/bin filesystem path, hence pertinent permissions are required for the user to alter it.
