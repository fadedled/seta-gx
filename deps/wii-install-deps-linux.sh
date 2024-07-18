
git clone https://github.com/rtissera/libchdr.git

cd libchdr/
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-cmake
make chdr-static
#move all generated files 
sudo rsync -av include/libchdr /opt/devkitpro/portlibs/ppc/include
sudo rsync -av include/dr_libs /opt/devkitpro/portlibs/ppc/include
sudo cp libchdr-static.a /opt/devkitpro/portlibs/ppc/lib/libchdr.a

sudo rsync -av deps/lzma*/include /opt/devkitpro/portlibs/ppc/include
sudo cp deps/lzma*/liblzma.a /opt/devkitpro/portlibs/ppc/lib/liblzma.a
sudo cp deps/zstd*/build/cmake/lib/libzstd.a /opt/devkitpro/portlibs/ppc/lib/libzstd.a


