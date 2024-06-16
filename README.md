# Avisor - A lightweight Hypervisor for Unikernel
## Introduction
Avisor is a lightweight, Type-1 hypervisor, running directly on the bare-metal hardware. Avisor is designed for unikernel, ensuring that the goal of a lightweight Hypervisor.
## Supported Architectures and Platforms
### Architecture Supported
- [x] Armv8-A aarch64
### Platform Supported
- [x] QEMU-Virt
## Getting Started
### Requirements
the following packages are required:
+ qemu-system-arm
+ gcc-aarch64-linux-gnu
+ gdb-multiarch

Make sure gcc supports the c11 standard.
### Building and Running
The Unikernel image file needs to be prepared and configured in advance in `config.c`.
```C
VM_IMAGE(vm1, "../app-helloworld_kvm-arm64.bin");
```
Then you can try `make` or `make qemu` to build and run.
### Debug
The way to debug avisor can be referred to the way to debug qemu. We use the `make debug` and `make telnet` commands for debugging. The specific steps are as follows:
1. Execute `make debug` in the working directory
2. Execute `make telnet` in the same directory