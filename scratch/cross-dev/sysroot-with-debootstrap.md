# Getting sysroot with debootstrap

Preparation
```
apt install qemu-user qemu-user-binfmt debootstrap
```

ARM64
```
export TSYSROOT=/lindata/dev_arm/sysroot_arm64_trixie

sudo debootstrap \
  --arch=arm64 \
  --variant=minbase \
  --foreign \
  trixie \
  "$TSYSROOT" \
  https://deb.debian.org/debian
```
ARMHF
```
export TSYSROOT=/lindata/dev_arm/sysroot_armhf_trixie

sudo debootstrap \
  --arch=armhf \
  --variant=minbase \
  --foreign \
  trixie \
  "$TSYSROOT" \
  https://deb.debian.org/debian
```

RV64
```
export TSYSROOT=/lindata/dev_riscv/sysroot_riscv64_trixie

sudo debootstrap \
  --arch=riscv64 \
  --variant=minbase \
  --foreign \
  trixie \
  "$TSYSROOT" \
  https://deb.debian.org/debian
```

```
sudo chroot "$TSYSROOT" /debootstrap/debootstrap --second-stage

sudo cp --dereference /etc/resolv.conf "$TSYSROOT/etc/resolv.conf"

```

For Ubuntu 24.04 (not required on Ubuntu 26.04)
```
sudo apt install debootstrap qemu-user-static binfmt-support
sudo cp /usr/bin/qemu-riscv64-static "$TSYSROOT/usr/bin/"
```

Enter the target:
```
sudo chroot "$TSYSROOT" bash
```
In the target/emulated sysroot:
```
apt update
apt install libc6-dev libstdc++-14-dev pkg-config
apt install default-libmysqlclient-dev
```