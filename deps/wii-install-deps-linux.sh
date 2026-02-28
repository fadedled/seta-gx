
#libjoy
if [ ! -d "libjoy" ]; then
	echo "libjoy does not exist."
	git clone https://github.com/fadedled/libjoy.git
	NEW_COPY=1
else
	echo "libjoy does exist."
	NEW_COPY=0
fi
cd libjoy/
make 
#move all needed
sudo rsync -av include/ /opt/devkitpro/portlibs/ppc/include
sudo cp lib/libjoy.a /opt/devkitpro/portlibs/ppc/lib/libjoy.a
cd ../

#libCHD
if [ ! -d "libchdr" ]; then
	echo "libchdr does not exist."
	git clone https://github.com/rtissera/libchdr.git
	NEW_COPY=1
else
	echo "libchdr does exist."
	NEW_COPY=0
fi

cd libchdr/

/opt/devkitpro/devkitPPC/bin/powerpc-eabi-cmake CMakeLists.txt -DZSTD_MULTITHREAD_SUPPORT=OFF
make chdr-static

#move all generated files
#libCHD
echo "Copy include for libchdr"
sudo rsync -av include/libchdr /opt/devkitpro/portlibs/ppc/include
echo "Copy include for dr_libs"
sudo rsync -av include/dr_libs /opt/devkitpro/portlibs/ppc/include
echo "Copy libchdr.a"
sudo cp libchdr-static.a /opt/devkitpro/portlibs/ppc/lib/libchdr.a

#LZMA
echo "Copy include for lzma include"
sudo rsync -av deps/lzma*/include /opt/devkitpro/portlibs/ppc/include
echo "Copy liblzma.a"
sudo cp deps/lzma*/liblzma.a /opt/devkitpro/portlibs/ppc/lib/liblzma.a
echo "Copy libzstd.a"
sudo cp deps/zstd*/build/cmake/lib/libzstd.a /opt/devkitpro/portlibs/ppc/lib/libzstd.a


