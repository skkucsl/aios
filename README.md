# Asynchronous I/O Stack

This is the source code repository for the paper entitled "Asynchronous I/O Stack: A Low-latency Kernel I/O Stack for Ultra-Low Latency SSDs" in the 2019 USENIX Annual Technical Conference

## How to use

* Enable `CONFIG_AIOS` and `CONFIG_INTEL_IOMMU`.
* Open a file using a `O_AIOS` or `040000000` flag. The file should be located in Ext4 file system and in an NVMe SSD.
* Access the file using `read(), pread(), write(), pwrite(), fsync() and fdatasync()`.

## Warning

* The source code is not tested with SSDs having a volatile write cache. All tested NVMe SSDs have the non-volatile write cache feature.
* The current implementation supports only one NVMe SSD at a time. If you have multiple NVMe SSDs, the `DEV_INSTANCE` in `drivers/nvme/host/pci.c` should be properly adjusted to specify an NVMe SSD you want to apply AIOS to.
* The current implementation does not support the `mmap()` path. 
