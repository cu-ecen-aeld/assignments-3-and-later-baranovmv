#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]; then
  echo "Using default directory ${OUTDIR} for output"
else
  OUTDIR=$1
  echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "linux-stable" ]; then
  #Clone only if the repository does not exist.
  echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
  git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e linux-stable/arch/${ARCH}/boot/Image ]; then
  pushd linux-stable
  echo "Checking out version ${KERNEL_VERSION}"
  git checkout ${KERNEL_VERSION}

  # TODO: Add your kernel build steps here
  make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
  make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
  make CCACHE_PREFIX="sccache" -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
  make CCACHE_PREFIX="sccache" -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
  make CCACHE_PREFIX="sccache" -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
  popd
fi
echo "Adding the Image in outdir"
cp linux-stable/arch/${ARCH}/boot/Image .

echo "Creating the staging directory for the root filesystem"
if [ -d "rootfs" ]; then
  echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
  sudo rm -rf rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs
pushd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
popd

if [ ! -d "busybox" ]; then
  git clone git://busybox.net/busybox.git
  cd busybox
  git checkout ${BUSYBOX_VERSION}
  # TODO:  Configure busybox
  make distclean
else
  cd busybox
fi

# TODO: Make and install busybox
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=../rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ..

echo
echo "Library dependencies"
pushd rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
CROSS_PATH="$(dirname `which  aarch64-none-linux-gnu-gcc`)/../aarch64-none-linux-gnu/libc"
cp ${CROSS_PATH}/lib64/libresolv.so.2 lib64
cp ${CROSS_PATH}/lib64/libm.so.6 lib64
cp ${CROSS_PATH}/lib64/libc.so.6 lib64
cp ${CROSS_PATH}/lib/ld-linux-aarch64.so.1 lib

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1
popd

# TODO: Clean and build the writer utility
pushd $FINDER_APP_DIR
CROSS_COMPILE=aarch64-none-linux-gnu- make clean && CROSS_COMPILE=aarch64-none-linux-gnu- make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
APP_FILES="autorun-qemu.sh finder.sh conf/ finder-test.sh writer"
echo "Copying $APP_FILES to $OUTDIR/rootfs/home"
cp -r $APP_FILES "$OUTDIR/rootfs/home"

# TODO: Chown the root directory
sudo chown -R 0:0 $OUTDIR/rootfs

# TODO: Create initramfs.cpio.gz
pushd $OUTDIR/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio
popd
