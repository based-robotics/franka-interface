# Get CPU core count
n_cores=$(nproc)

echo "Using CONDA_PREFIX = '$CONDA_PREFIX'"
ls -l "$CONDA_PREFIX/lib/libfranka.so"* || echo "  → no libfranka.so under $CONDA_PREFIX/lib"

[ -d build ] || mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. \
    -DCMAKE_CXX_FLAGS="-Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable" \
    -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
    -DCMAKE_PREFIX_PATH="$CONDA_PREFIX" \
    -DCMAKE_INCLUDE_PATH="$CONDA_PREFIX/include" \
    -DCMAKE_INSTALL_LIBDIR="$CONDA_PREFIX/lib"

make -j$n_cores 
cd ..