shapelib project
----------------

This is a `fork` of a `clone` of the orginal **cvs** repository.

Building on Unix
----------------

 1) $ cd build # get into the out-of-source build folder
 2) $ cmake .. [options] # to generate native Makefiles
 3) $ make # build the project
 4) $ if desireable, $ [sudo] make install # to install

Building on Windows
-------------------

If you have run the VC++ VCVARS32.BAT, you should be able to type the 
following in the command window to build the code and executables:

```
 > cd build # get into the out-of-source build folder
 > cmake .. [options] # to generate native build files
 > cmake --build . --config Release # build the project
 > cmake --build . --config Release --taqrget INSTALL # If you have set an instal l location
```

The `build` folder contains a **build-me.bat**, which should work if you have MSVC14 2015 install, or as modified for your environment, MSVC install.

CMake Options include:
---------------------

 - -DCMAKE_BUILD_TYPE=Release|Debug = Quite important in unix
 - -DCMAKE_INSTALL_PREFIX:PATH=X:/3rdParty.x64 = Set the install root location

; eof
