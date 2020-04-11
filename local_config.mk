ifeq ($(TARGET_OS),windows)
# `-femulated-tls` seems to be necessary when using libstdc++'s atomics on Windows.
# If using official clang binaries, add `--target=x86_64-w64-windows-gnu`.
C_COMPILER   = clang -femulated-tls
CXX_COMPILER = clang++ -femulated-tls
C_LINKER     = clang -femulated-tls -fuse-ld=lld
CXX_LINKER   = clang++ -femulated-tls -fuse-ld=lld
else
C_COMPILER   = clang-9
CXX_COMPILER = clang++-9
C_LINKER     = clang-9 -fuse-ld=lld
CXX_LINKER   = clang++-9 -fuse-ld=lld
endif
