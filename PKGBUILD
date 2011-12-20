# Maintainer: Ashley Anderson <amanderson@albinoloverats.net>
# Contributor: Ashley Anderson <amanderson@albinoloverats.net>
pkgname=stegfs
pkgver=201101
pkgrel=1
pkgdesc=(A Fuse based file system which provides absolute security. Using encryption to secure files, and the art of steganography to hide them, stegfs aims to ensure that the existence of such files cannot be proven.)
arch=(i686 x86_64)
url=(https://albinoloverats.net/stegfs)
license=(GPL)
groups=()
depends=(fuse libmcrypt mhash)
makedepends=(pkgconfig)
provides=()
conflicts=()
replaces=()
backup=()
options=()
install=
source=()
noextract=()
md5sums=()

build() {
  cd ..
  mkdir -p pkg/usr/{bin,lib,man/man1,share/{applications,locale/de/LC_MESSAGES}}
  make
  make install PREFIX=pkg
}
