# Thanks to Mehmet Erol Sanliturk for creating this.
declare -x Current_Directory=`pwd`
declare -x Jobs=4

mkdir LLVM_Binaries
mkdir build-llvm

git clone https://github.com/Leporacanthicus/lacsap.git lacsap
source lacsap/llvmversion

cd ${Current_Directory}

cd build-llvm
# cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local/llvm -DLLVM_TARGETS_TO_BUILD=X86 ../llvm
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${Current_Directory}/LLVM_Binaries -DLLVM_TARGETS_TO_BUILD=X86 ../llvm
make -i -k -j ${Jobs}
make -i -k check-llvm
make -i -k install

cd ${Current_Directory}

cd lacsap
rm -f -R *.o

make -i -k USECLANG=0  M32=0  LLVM_DIR=${Current_Directory}/LLVM_Binaries 
make -i -k fulltests
