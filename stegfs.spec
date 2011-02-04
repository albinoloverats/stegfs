Summary: A fuse based steganographic file system
Name: stegfs
Version: 201101
Release: 1
Source: https://albinoloverats.net/downloads/%{name}.tar.bz2
URL: https://albinoloverats.net/%{name}
License: GPL
BuildRoot: /var/tmp/%{name}
Group: Applications/File

%description
stegfs is a Fuse based file system which provides absolute security.
Using encryption to secure files, and the art of steganography to
hide them, stegfs aims to ensure that the existence of such files
isn't guaranteed. Implemented as a Fuse based file system and using
the mhash and mcrypt libraries to provide the cryptographic hash and
symmetric block cipher functions, stegfs is at the cutting edge of
secure file system technology.
 
%prep
%setup -q

%build
make

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/{bin,man/man1,share/locale/de/LC_MESSAGES}
make install PREFIX=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
/usr/bin/stegfs
/usr/bin/mkstegfs
/usr/man/man1/stegfs.1.gz
/usr/share/locale/de/LC_MESSAGES/stegfs.mo
