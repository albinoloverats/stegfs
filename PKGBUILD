# Maintainer: Ashley Anderson <amanderson@albinoloverats.net>
# Contributor: Ashley Anderson <amanderson@albinoloverats.net>
pkgname=stegfs
pkgver=2015.06
pkgrel=1
pkgdesc="stegfs is a FUSE based file system which provides absolute security. Using encryption to secure files, and the art of steganography to hide them, stegfs aims to ensure that the existence of such files isn't guaranteed. Implemented as a FUSE based file system and using the mhash and mcrypt libraries to provide the cryptographic hash and symmetric block cipher functions, stegfs is at the cutting edge of secure file system technology."
url="https://albinoloverats.net/projects/stegfs"
arch=('i686' 'x86_64' 'arm')
license=('GPL3')
depends=('fuse' 'libmcrypt' 'mhash')
makedepends=('pkgconfig')

# you shouldn't need to uncomment this as this PKGBUILD file lives in
# the same Git repoository as the source
# source=(https://albinoloverats.net/downloads/stegfs.tar.xz)
# sha256sum=('')

build() {
  cd ${startdir}
  make -f Makefile
}

package() {
  cd ${startdir}
  mkdir -p ${pkgdir}/usr/{bin,share/man/man1}
  make -f Makefile install PREFIX=${pkgdir}
}
