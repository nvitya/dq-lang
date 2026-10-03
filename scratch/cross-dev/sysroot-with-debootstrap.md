# Getting sysroot with debootstrap

```
apt install qemu-user qemu-user-binfmt debootstrap

export TSYSROOT=/lindata/dev_arm/sysroot_arm64_trixie

sudo debootstrap \
  --arch=arm64 \
  --variant=minbase \
  --foreign \
  trixie \
  "$TSYSROOT" \
  https://deb.debian.org/debian


sudo chroot "$TSYSROOT" /debootstrap/debootstrap --second-stage

sudo cp --dereference /etc/resolv.conf "$TSYSROOT/etc/resolv.conf"

```

Enter the target:
```
export TSYSROOT=/lindata/dev_arm/sysroot_arm64_trixie
sudo chroot "$TSYSROOT" bash
```
In the target/emulated sysroot:
```
apt update
apt install libc6-dev libstdc++-14-dev pkg-config
apt install default-libmysqlclient-dev
```