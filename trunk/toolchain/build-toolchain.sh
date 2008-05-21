#!/bin/sh

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

#LOG FILE
BUILDLOG=build.log

# display usage
Usage () 
{ 
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
	echo " got ret $1"
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
if [ -z "$OIBOOT_PATH" ];
then
  OIBOOT_PATH="/tmp/openiboot"
fi

if [ $CMD_TYPE -gt 1 ];
then
	echo -en "Removing temporary files\n"
	rm -rf $OIBOOT_PATH
	checkRet $? "Failed to remove $OIBOOT_PATH/src"
	echo -en "Done\n"
	exit 0
fi

echo -en "=======================================\n"

# Create tmp dirs
# BinUtils
echo -en "Creating default directories\n"
mkdir -p $OIBOOT_PATH/binutils-build
checkRet $? "failed to create $OIBOOT_PATH/binutils-build" $EXIT_TRUE
echo "- $OIBOOT_PATH/binutils-build"
# GCC
mkdir -p $OIBOOT_PATH/gcc-build
checkRet $? "failed to create $OIBOOT_PATH/gcc-build" $EXIT_TRUE
echo "- $OIBOOT_PATH/gcc-build"
# New lib
mkdir -p $OIBOOT_PATH/newlib-build
checkRet $? "failed to create $OIBOOT_PATH/newlib-build" $EXIT_TRUE
echo "- $OIBOOT_PATH/newlib-build"
# Package src dir
mkdir -p $OIBOOT_PATH/src/
checkRet $? "failed to create $OIBOOT_PATH/src" $EXIT_TRUE
echo "- $OIBOOT_PATH/src"

echo -en "Downloading packages\n"
# BinUtils
checkFile $OIBOOT_PATH/$PKG_BINUTILS
checkRet $? "- $PKG_BINUTILS complete" $EXIT_FALSE
wget $PKG_MIRROR/$PKG_BINUTILS -O $OIBOOT_PATH/src/$PKG_BINUTILS >> $OIBOOT_PATH/$BUILDLOG 2>&1
echo "- $PKG_BINUTILS complete"
# GCC
checkFile $OIBOOT_PATH/$PKG_GCC411
checkRet $? "Failed to retrive $PKG_GCC411" $EXIT_TRUE
wget $PKG_MIRROR/$PKG_GCC411 -O $OIBOOT_PATH/src/$PKG_GCC411 >> $OIBOOT_PATH/$BUILDLOG 2>&1
echo "- $PKG_GCC411 complete"
# NewLib
checkFile $OIBOOT_PATH/$PKG_NEWLIB
checkRet $? "Failed to retrive $PKG_NEWLIB" $EXIT_TRUE
wget $PKG_MIRROR/$PKG_NEWLIB -O $OIBOOT_PATH/src/$PKG_NEWLIB >> $OIBOOT_PATH/$BUILDLOG 2>&1
echo "- $PKG_NEWLIB complete"

echo -en "Starting package build/install\n"

echo -en "- Extracting binutils\n"
cd $OIBOOT_PATH
tar -jxvf $OIBOOT_PATH/src/$PKG_BINUTILS >> $OIBOOT_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_BINUTILS" $EXIT_TRUE

echo -en "- Doing binutils configure\n"
cd $OIBOOT_PATH/binutils-build
../binutils-2.17/configure --target=arm-elf --prefix=/usr/local \
	--enable-interwork --enable-multilib >> $OIBOOT_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to configure binutils" $EXIT_TRUE

echo -en "- Starting binutils build\n"
make all >> $OIBOOT_PATH/$BUILDLOG 2>&1

echo -en "- Installing binutils\n"
make install >> $OIBOOT_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- Binutils Completed\n"

echo -en "- Extracting GCC\n"
cd $OIBOOT_PATH
tar -jxvf $OIBOOT_PATH/src/$PKG_GCC411 >> $OIBOOT_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_GCC411" $EXIT_TRUE
echo -en "- Extracting Newlib depednacy for gcc\n"

tar -jxvf $OIBOOT_PATH/src/$PKG_NEWLIB >> $OIBOOT_PATH/$BUILDLOG 2>&1
checkRet $? "Failed to extract package $PKG_NEWLIB" $EXIT_TRUE

echo -en "- Doing GCC configure\n"
cd gcc-build
../gcc-4.1.1/configure --target=arm-elf --prefix=/usr/local \
    --enable-interwork --enable-multilib \
    --enable-languages="c,c++" --with-newlib \
    --with-headers=../newlib-1.14.0/newlib/libc/include
checkRet $? "Failed to configure gcc" $EXIT_TRUE

echo -en "- Starting GCC build\n"
make all-gcc >> $OIBOOT_PATH/$BUILDLOG 2>&1

echo -en "- Installing GCC\n"
make install-gcc >> $OIBOOT_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- GCC Part 1 Completed\n"

echo -en "- Doing NewLib configure\n"
cd newlib-build
checkRet $? "Failed to configure newlib" $EXIT_TRUE
../newlib-1.14.0/configure --target=arm-elf --prefix=/usr/local \
	--enable-interwork --enable-multilib

echo -en "- Starting NewLib build\n"
make all >> $OIBOOT_PATH/$BUILDLOG 2>&1

echo -en "- Installing NewLib\n"
make install >> $OIBOOT_PATH/$BUILDLOG 2>&1
cd ../
echo -en "- NewLib Completed\n"
echo "- Doing part 2 of GCC build"
cd gcc-build
echo "- Making all GCC"
make all >> $OIBOOT_PATH/$BUILDLOG 2>&1
echo "- Installing GCC\n"
make install >> $OIBOOT_PATH/$BUILDLOG 2>&1
echo "- GCC part 2 comlete"
echo -en "Cleaning up sources\n"
echo -en "Toolchain install successful\n"
echo -en "=======================================\n"
