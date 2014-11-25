# Maintainer: Ashley Anderson <amanderson@albinoloverats.net>
# Contributor: Ashley Anderson <amanderson@albinoloverats.net>
pkgname=stegfs
pkgver=2014.00
pkgrel=1
pkgdesc="A FUSE based file system which provides absolute security. Using encryption to secure files, and the art of steganography to hide them, stegfs aims to ensure that the existence of such files cannot be proven."
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
