in Cmakelists.txt<br>
change this where your raylib include and lib is located<br>
include_directories(../raylib/include)<br>
link_directories(../raylib/)<br>

to build<br>

mkdir build && cd build<br>
cmake ..<br>
make<br>
