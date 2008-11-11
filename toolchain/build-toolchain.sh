#!/bin/bash

#
# iPhoneLinux.org Toolchain builder
#
#

# Package URL
PKG_MIRROR="http://www.gnuarm.com"

# Package Verions
PKG_BINUTILS="binutils-2.17.tar.bz2"
PKG_GCC411="gcc-4.1.1.tar.bz2"
PKG_NEWLIB="newlib-1.14.0.tar.gz"

# Package Patches
PATCH_MIRROR="http://www.iphonelinux.org"
PATCH_GCC411_ARMELF="t-arm-elf.patch"
PATCH_NEWLIB_MAKEINFO="newlib-1.14.0-missing-makeinfo.patch"


#LOG FILE
BUILDLOG=build.log

# display usage
Usage () 
{ 
	if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
	fi

	if [ -z "$1" ];
	then
		echo "Usage: ./build-toolchain.sh make|clean"
		exit 1
	elif [ "$1" == "make" ];
		then
			return 1
	elif [ "$1" == "clean" ];
		then
			return 2
	fi
}
# Check file 
checkFile()
{
	if [ -e $1 ];
	then
		return 1;
	fi
	return 0;
}
# Check return code
EXIT_TRUE=1
EXIT_FALSE=0
checkRet ()
{
	if [ $1 -ne 0 ];
	then
  	echo $2
		if [ $3 -eq $EXIT_TRUE ];
		then
  		exit 1
		fi
	fi
}
# Show usage
Usage $1
CMD_TYPE=$?

# Check for different prefix
if [ -z "$TOOLCHAIN_PATH" ];
then
  TOOLCHAIN_PATH="/tmp/ipl-toolchain"
fi

if [ $CMD_TYPE -gt 1 ];
then
	echo -en "Removing temporary files\n"
	rm -rf $TOOLCHAIN_PATH
	checkRet $? "Failed to remove $TOOLCHAIN_PATH/src"
	echo -en "Done\n"
	exit 0
fi

echo -en "=======================================\n"

# Create tmp dirs
# BinUtils
echo -en "Creating default directories\n"
mkdir -p $TOOLCHAIN_PATH/binutils-build
checkRet $? "failed to create $TOOLCHAIN_PATH/binutils-build" $EXIT_TRUE
echo "- $TOOLCHAIN_PATH/binutils-build"
# GCC
mkdir -p $TOOLCHAIN_PATH/gcc-build
checkRet $? "failed to create $TOOLCHAIN_PATH/gcc-build" $EXIT_TRUE
echo "- $TOOLCHAIN_PATH/gcc-build"
# New lib
mkdir -p $TOOLCHAIN_PATH/newlib-build
checkRet $? "failed to create $TOOLCHAIN_PATH/newlib-build" $EXIT_TRUE
echo "- $TOOLCHAIN_PATH/newlib-build"
# Package src dir
mkdir -p $TOOLCHAIN_PATH/src/
checkRet $? "failed to create $TOOLCHAIN_PATH/src" $EXIT_TRUE
echo "- $TOOLCHAIN_PATH/src"

# Create log file
echo "" > $TOOLCHAIN_PATH/$BUILDLOG 2>&1
echo -en "Downloading packages\n"
# BinUtils
echo -en "- Downloading $PKG_BINUTILS\n"
checkFile $TOOLCHAIN_PATH/src/$PKG_BINUTILS
if [ $? -eq 0 ];
then
	wget $PKG_MIRROR/$PKG_BINUTILS -O $TOOLCHAIN_PATH/src/$PKG_BINUTILS >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
	checkRet $? "Failed to retrive $PKG_BINUTILS" $EXIT_TRUE
fi
echo "- $PKG_BINUTILS download complete"
# GCC
echo -en "- Downloading $PKG_GCC411\n"
checkFile $TOOLCHAIN_PATH/src/$PKG_GCC411
if [ $? -eq 0 ];
then
  wget $PKG_MIRROR/$PKG_GCC411 -O $TOOLCHAIN_PATH/src/$PKG_GCC411 >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
  checkRet $? "Failed to retrive $PKG_GCC411" $EXIT_TRUE
fi
echo "- $PKG_GCC411 download complete"
# NewLib
echo -en "- Downloading $PKG_NEWLIB\n"
checkFile $TOOLCHAIN_PATH/src/$PKG_NEWLIB
if [ $? -eq 0 ];
then
  wget $PKG_MIRROR/$PKG_NEWLIB -O $TOOLCHAIN_PATH/src/$PKG_NEWLIB >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
  checkRet $? "Failed to retrive $PKG_NEWLIB" $EXIT_TRUE
fi
echo "- $PKG_NEWLIB download complete"

echo -en "Starting package build/install\n"
echo -en "- Extracting binutils\n"
cd $TOOLCHAIN_PATH
tar -jxvf $TOOLCHAIN_PATH/src/$PKG_BINUTILS >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_BINUTILS" $EXIT_TRUE

echo -en "- Doing binutils configure\n"
cd $TOOLCHAIN_PATH/binutils-build
../binutils-2.17/configure --target=arm-elf --prefix=/usr/local \
	--enable-interwork --enable-multilib --disable-werror >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to configure binutils" $EXIT_TRUE

echo -en "- Starting binutils build\n"
make all >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1

echo -en "- Installing binutils\n"
make install >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- Binutils Completed\n"

echo -en "- Extracting GCC\n"
cd $TOOLCHAIN_PATH
tar -jxvf $TOOLCHAIN_PATH/src/$PKG_GCC411 >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_GCC411" $EXIT_TRUE
echo -en "- Extracting Newlib depednacy for gcc\n"

tar -zxvf $TOOLCHAIN_PATH/src/$PKG_NEWLIB >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_NEWLIB" $EXIT_TRUE

echo -en "- Downloading t-arm-elf patch\n" 
wget -r $PATCH_MIRROR/$PATCH_GCC411_ARMELF -O $TOOLCHAIN_PATH/t-arm-elf.patch >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to download patch $PATCH_GCC411_ARMELF" $EXIT_TRUE
patch -p0 < $PATCH_GCC411_ARMELF >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to apply patch for t-arm-elf" $EXIT_TRUE
echo -en "- Doing GCC configure\n"
cd gcc-build
../gcc-4.1.1/configure --target=arm-elf --prefix=/usr/local \
    --enable-interwork --enable-multilib \
    --enable-languages="c,c++" --with-newlib \
    --with-headers=../newlib-1.14.0/newlib/libc/include --disable-werror >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to configure gcc" $EXIT_TRUE

echo -en "- Starting GCC build\n"
make all-gcc >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1

echo -en "- Installing GCC\n"
make install-gcc >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- GCC Part 1 Completed\n"

echo -en "- Downloading newlib makeinfo patch\n" 
wget -r $PATCH_MIRROR/$PATCH_NEWLIB_MAKEINFO -O $TOOLCHAIN_PATH/$PATCH_NEWLIB_MAKEINFO >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to download patch $PATCH_NEWLIB_MAKEINFO" $EXIT_TRUE
patch -p0 < $PATCH_NEWLIB_MAKEINFO >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to apply patch for newlib makeinfo" $EXIT_TRUE
echo -en "- Doing NewLib configure\n"
cd newlib-build
checkRet $? "Failed to configure newlib" $EXIT_TRUE
../newlib-1.14.0/configure --target=arm-elf --prefix=/usr/local \
	--enable-interwork --enable-multilib --disable-werror >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1

echo -en "- Starting NewLib build\n"
make all >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1

echo -en "- Installing NewLib\n"
make install >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- NewLib Completed\n"

echo "- Doing part 2 of GCC build"
cd gcc-build
echo "- Making all GCC"
make all >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
echo "- Installing GCC"
make install >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1

echo "- GCC part 2 complete"
echo -en "Toolchain install successful\n"
echo -en "=======================================\n"
