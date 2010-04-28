#!/bin/bash

#
# iPhoneLinux.org Toolchain builder
#
# on ubuntu install the following packages : build-essential texinfo

#########  Setup Variables  ###########
MYDIR="$PWD/`dirname $0`"
declare -i CPU="$(cat /proc/cpuinfo | grep processor | wc -l) + 1"
# Package URL
PKG_MIRROR="http://www.gnuarm.com"

# Package Verions
PKG_BINUTILS="binutils-2.17.tar.bz2"
PKG_GCC411="gcc-4.1.1.tar.bz2"
PKG_NEWLIB="newlib-1.14.0.tar.gz"

# Package Patches
PATCH_GCC411_ARMELF="t-arm-elf.patch"
PATCH_NEWLIB_MAKEINFO="newlib-1.14.0-missing-makeinfo.patch"


if [ -z "$IPHONELINUXDEV" ]; then
	PREFIX=/usr/local
	NEEDROOT=1
else
	PREFIX="$IPHONELINUXDEV"
	NEEDROOT=0
fi
export PATH="$PATH:$PREFIX/bin"

# Check for different toolchain prefix
if [ -z "$TOOLCHAIN_PATH" ]; then
  TOOLCHAIN_PATH="/tmp/ipl-toolchain"
fi

#LOG FILE
BUILDLOG=build.log



#########  Helper functions  ###########

usage() {
	echo "Usage: ./build-toolchain.sh clean | make [stage]"
	echo "  if stage is included the build will begin from"
	echo "  there instead of from the beginning"
	exit 1
}

# Holds the value of the current stage so that checkRet can echo it if it fails
CURRENT_STAGE=""

checkRet() {
	# Die if return code != 0
	if [ $? -ne 0 ]; then
		if [ -n "$CURRENT_STAGE" ]; then
			echo "$1 (stage: $CURRENT_STAGE)"
		else
			echo "$1"
		fi
  		exit 1
	fi
}

log() {
	#execute command redirecting to the log file
	"$@" >> $TOOLCHAIN_PATH/$BUILDLOG 2>&1
}
# Create log file
echo > $TOOLCHAIN_PATH/$BUILDLOG

STAGE=$2
STAGE_MSG=""
stage() {
	# if $STAGE is not empty then only execute stage if it matches, clearing afterwards
	# Sets $CURRENT_STAGE and executes the stage
	if [ -n "$STAGE" -a "$STAGE" != "$1" ]; then
		STAGE_MSG=""
		return 0
	fi
	STAGE=""
	CURRENT_STAGE="$1"
	
	echo -en "$STAGE_MSG"
	STAGE_MSG=""
	stage_$1
}

#Stores a message that will be printed before the next stage
#if the next stage is run
msg() {
	STAGE_MSG="$STAGE_MSG$@\n"
}


#########  Quick tests  ###########

if [ "$NEEDROOT" == "1" -a "$(id -u)" != "0" ]; then
	echo "This script must be run as root" 1>&2
	exit 1
fi

# check for make or clean
case "$1" in
	make);; #good, move on through
	clean)
		echo "Removing temporary files"
		rm -rf $TOOLCHAIN_PATH
		checkRet "Failed to remove $TOOLCHAIN_PATH"
		echo "Done"
		exit 0
		;;
	*) #bad
		usage
esac


#########  Build stages  ###########

stage_setup() {
	# Create tmp dirs
	echo "- Creating default directories"
	cd $MYDIR
	for dir in binutils-build gcc-build newlib-build src; do
		mkdir -p $TOOLCHAIN_PATH/$dir
		checkRet "failed to create $TOOLCHAIN_PATH/$dir"
		echo "  - $TOOLCHAIN_PATH/$dir"
	done

	# Copy patch files
	cp $MYDIR/*.patch $TOOLCHAIN_PATH
}

stage_download() {
	echo "- Downloading packages"

	for pkg in "$PKG_BINUTILS" "$PKG_GCC411" "$PKG_NEWLIB"; do
	
		if [ -e $TOOLCHAIN_PATH/src/$pkg ];
		then
			echo "  - $pkg exists"
		else
			# Download to a .tmp file so that if the download is interupted
			# we don't think that the file is downloaded
			echo "  - Downloading $pkg"
			log wget $PKG_MIRROR/$pkg -O $TOOLCHAIN_PATH/src/$pkg.tmp
			checkRet "Failed to retrive $pkg"
			mv $TOOLCHAIN_PATH/src/$pkg{.tmp,} # move file.tmp to file
			echo "  - $pkg download complete"
		fi
	done
}

stage_binutils_extract() {
	echo "- Extracting binutils"
	cd $TOOLCHAIN_PATH
	log tar -jxvf $TOOLCHAIN_PATH/src/$PKG_BINUTILS
	checkRet "Failed to extract package $PKG_BINUTILS"
}

stage_binutils_configure() {
	echo "- Configuring binutils"
	cd $TOOLCHAIN_PATH/binutils-build
	log ../binutils-2.17/configure --target=arm-elf --prefix=$PREFIX \
			--enable-interwork --enable-multilib --disable-werror
	checkRet "Failed to configure binutils"
}

stage_binutils_build() {
	echo "- Building binutils"
	cd $TOOLCHAIN_PATH/binutils-build
	log make -j$CPU all
	checkRet "Failed to build binutils"
}

stage_binutils_install() {
	echo "- Installing binutils"
	cd $TOOLCHAIN_PATH/binutils-build
	log make -j$CPU install
	checkRet "Failed to install binutils"
}

stage_gcc_extract() {
	echo "- Extracting GCC"
	cd $TOOLCHAIN_PATH
	log tar -jxvf $TOOLCHAIN_PATH/src/$PKG_GCC411
	checkRet "Failed to extract package $PKG_GCC411"
}

stage_newlib_extract() {
	echo "- Extracting Newlib dependency for gcc"
	cd $TOOLCHAIN_PATH
	log tar -zxvf $TOOLCHAIN_PATH/src/$PKG_NEWLIB
	checkRet "Failed to extract package $PKG_NEWLIB"
}

stage_gcc_patch() {
	echo "- Patching GCC for t-arm-elf"
	cd $TOOLCHAIN_PATH
	log patch -p0 < $PATCH_GCC411_ARMELF
	checkRet "Failed to apply patch for t-arm-elf"
}

stage_gcc_configure() {
	echo "- Configuring GCC"
	cd $TOOLCHAIN_PATH/gcc-build
	log ../gcc-4.1.1/configure --target=arm-elf --prefix=$PREFIX \
			--enable-interwork --enable-multilib --with-fpu=vfp \
			--enable-languages="c,c++" --with-newlib \
			--with-headers=../newlib-1.14.0/newlib/libc/include --disable-werror
	checkRet "Failed to configure gcc"
}

stage_gcc_build() {
	echo "- Building GCC part 1"
	cd $TOOLCHAIN_PATH/gcc-build
	log make -j$CPU all-gcc
	checkRet "Failed to build GCC part 1"
}

stage_gcc_install() {
	echo "- Installing GCC part 1"
	cd $TOOLCHAIN_PATH/gcc-build
	log make -j$CPU install-gcc
	checkRet "Failed to install GCC part 1"
}

stage_newlib_patch() {
	echo "- Patching Newlib for makeinfo"
	cd $TOOLCHAIN_PATH
	log patch -p0 < $PATCH_NEWLIB_MAKEINFO
	checkRet "Failed to apply patch for newlib makeinfo"
}

stage_newlib_configure() {
	echo "- Configuring Newlib"
	cd $TOOLCHAIN_PATH/newlib-build
	log ../newlib-1.14.0/configure --target=arm-elf --prefix=$PREFIX \
		--enable-interwork --enable-multilib --disable-werror
	checkRet "Failed to configure newlib"
}

stage_makesymlink() {
	echo "- Making arm-elf-cc symlink"
	cd $TOOLCHAIN_PATH/newlib-build
	ln -s arm-elf-gcc $PREFIX/bin/arm-elf-cc
	checkRet "Failed to create symlink"
}

stage_newlib_build() {
	echo "- Building Newlib"
	cd $TOOLCHAIN_PATH/newlib-build
	log make -j$CPU all
	checkRet "Failed to build newlib"
}

stage_newlib_install() {
	echo "- Installing NewLib"
	cd $TOOLCHAIN_PATH/newlib-build
	log make -j$CPU install
	checkRet "Failed to install newlib"
}

stage_gcc_build2() {
	echo "- Building GCC part 2"
	cd $TOOLCHAIN_PATH/gcc-build
	log make -j$CPU all
	checkRet "Failed to build GCC part 2"
}

stage_gcc_install2() {
	echo "- Installing GCC part 2"
	cd $TOOLCHAIN_PATH/gcc-build
	log make -j$CPU install
	checkRet "Failed to install GCC part 2"
}

echo "======================================="
stage setup
stage download

msg "Starting Binutils"

stage binutils_extract
stage binutils_configure
stage binutils_build
stage binutils_install

msg "Completed Binutils"

msg "Starting GCC Part 1"

stage gcc_extract
stage newlib_extract

stage gcc_patch
stage gcc_configure
stage gcc_build
stage gcc_install

msg "Completed GCC Part 1"

msg "Starting Newlib"

stage newlib_patch
stage newlib_configure

stage makesymlink

stage newlib_build
stage newlib_install

msg "Completed NewLib"

msg "Starting GCC Part 2"
stage gcc_build2
stage gcc_install2

if [ -n "$STAGE" ]; then
	echo "Error: unknown stage $STAGE"
	usage
fi

echo "Completed GCC Part 2"
echo "Toolchain install successful"
echo "======================================="
echo
