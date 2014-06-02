<big>SSD Dashboard and Toolkit/CryptoErase Tool open source</big>

This repository contains the open source packages used in the SanDisk SSD Dashboard, SSD Toolkit, and CryptoErase Tools. All of the packages are subject to the license agreements included in each package. Where SanDisk modified the build configuration of a package, the modified build configuration is included in this repository.

The <i>legacy</i> tree is based on the buildroot Linux distribution, also available at http://buildroot.org. Packages in the buildroot/dl directory were taken directly from their original location and are included for completeness. The Grub4DOS and grub utilities are from the Ubuntu wubi tool.

The <i>uefi</i> tree is based on Ubuntu and includes packages taken from the Ubuntu Launchpad. The kernel used is the unmodified signed Ubuntu kernel with Secure Boot support, and the binaries in the <i>uefi/fstree</i> directory are identical to the ones distributed by Canonical. The copies maintained in this tree of Ubuntu packages represent the version of each package used in our tools. The latest version of each package is available at https://launchpad.net.

Scripts in this open source tree may reference proprietary SanDisk utilities, which are not included in this release but are available within the SSD Dashboard, SSD Toolkit and SanDisk CryptoErase tool packages. 

Please contact opensource@sandisk.com with any issues or questions.
