Summary: A fuse based steganographic file system
Name: vstegfs
Version: 200910
Release: 1
Source: https://albinoloverats.net/downloads/%{name}.tar.bz2
URL: https://albinoloverats.net/%{name}
License: GPL
BuildRoot: /var/tmp/%{name}
Group: Applications/File

%description
vstegfs is a steganographic file system in userspace which uses the FUSE
library. Steganographioc file systems are one step above traditional
encrypted file systems because they grant the user plausible
deniability.
 
%prep
%setup -q

%build
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/{bin,man/man1}
make PREFIX=%{buildroot}

%install
make install PREFIX=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
/usr/bin/vstegfs
/usr/bin/mkvstegfs
/usr/man/man1/vstegfs.1.gz
