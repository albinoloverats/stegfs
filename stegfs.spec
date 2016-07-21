Summary: A fuse based steganographic file system
Name: stegfs
Version: 2015.08.1
Release: 1
Source: https://albinoloverats.net/downloads/%{name}.tar.xz
URL: https://albinoloverats.net/projects/%{name}
License: GPL
BuildRoot: /var/tmp/%{name}
Group: Applications/File

%description
stegfs is a FUSE based file system which provides absolute security.
Using encryption to secure files, and the art of steganography to hide
them, stegfs aims to ensure that the existence of such files isn't
guaranteed. Implemented as a FUSE based file system and using the GNU
crypto library, libgcrypt, to provide the cryptographic hash and
symmetric block cipher functions, stegfs is at the cutting edge of
secure file system technology.

%global debug_package %{nil}

%prep
%setup -q

%build
make

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/{bin,share/man/man1}
make install PREFIX=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
/usr/bin/stegfs
/usr/bin/mkstegfs
/usr/share/man/man1/stegfs.1.gz
/usr/share/man/man1/mkstegfs.1.gz
