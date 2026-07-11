Docker container preparation:
```
docker pull ubuntu:24.04
docker run --name dq-ubuntu2404 -it ubuntu:24.04 bash

apt update
apt install -y lsb-release wget gnupg software-properties-common ca-certificates sudo
apt install -y build-essential git cmake ninja-build
```

Ubuntu 24.04 requirements for the light release package:
```
apt-get install -y clang lld g++ build-essential

```