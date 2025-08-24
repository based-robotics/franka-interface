cd third_party/libfranka

# Apply diff for libfranka 
git apply ../../bash_scripts/libfranka.diff

# Get CPU core count
n_cores=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')

# Build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
      -DCMAKE_PREFIX_PATH="$CONDA_PREFIX" \
      -DCMAKE_INCLUDE_PATH="$CONDA_PREFIX/include" \
      -DCMAKE_INSTALL_LIBDIR="$CONDA_PREFIX/lib" \
      -DBUILD_TESTING=OFF \
      -DCMAKE_CXX_STANDARD=17 ..

cmake --build . --target install -j$n_cores

# cd ../..

# # Copy needed files from libfranka cmake. -n means don't copy if dest. file exists
# [ -d cmake ] || mkdir cmake
# cp -n libfranka/cmake/FindEigen3.cmake cmake/
# cp -n libfranka/cmake/FindPoco.cmake cmake/
