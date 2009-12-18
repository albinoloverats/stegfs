# Contributor: Ashley Anderson <amanderson@albinoloverats.net>
pkgname=vstegfs
pkgver=201001
pkgrel=1
pkgdesc="vstegfs is a steganographic file system in userspace which uses the FUSE library. Steganographioc file systems are one step above traditional encrypted file systems because they grant the user plausible deniability."
arch=(i686 x86_64)
url="https://albinoloverats.net/vstegfs"
license=('GPL')
groups=()
depends=('fuse' 'libmcrypt' 'mhash')
makedepends=('pkgconfig')
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
