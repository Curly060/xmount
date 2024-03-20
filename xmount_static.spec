%define debug_package %{nil}

Name:			xmount
Summary:		Tool to crossmount between multiple input and output harddisk images
Version:		1.1.0
Release:		1
License:		GPL
Group:			Applications/System
URL:			https://code.sits.lu/foss/xmount
Source0:		%{name}-%{version}.tar.gz
Buildroot:		%{_tmppath}/%{name}-%{version}-%{release}-root
Requires:		fuse zlib
BuildRequires:		cmake fuse-devel zlib-devel openssl-devel openssl-static

%description
xmount allows you to convert on-the-fly between multiple input and output
harddisk image types. xmount creates a virtual file system using FUSE
(Filesystem in Userspace) that contains a virtual representation of the input
image. The virtual representation can be in raw DD, Apple's Disk Image format (DMG),
VirtualBox's virtual disk file format (VDI), VmWare's VMDK file format or Microsoft's
Virtual Hard Disk Image format (VHD). Input images can be raw DD, EWF
(Expert Witness Compression Format), AFF (Advanced Forensic Format v3 & v4),
VDI (VirtualBox Virtual Disk Image) or QCOW (QEMU Copy On Write) files.
In addition, xmount also supports virtual write access to the output files
that is redirected to a cache file. This makes it possible to boot acquired
harddisk images using QEMU, KVM, VirtualBox, VmWare or alike.

%prep
%setup -q

%build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSTATIC=1 -DLINUX_DIST=rhel7 ..
make %{?_smp_mflags}

%install
cd build
%{__make} DESTDIR=%{buildroot} install

%clean
rm -fr %{buildroot}

%post

%preun

%postun

%files
%defattr(-,root,root) 
%{_bindir}/*
%{_mandir}/*
%{_exec_prefix}/lib/%{name}/*.so
%doc AUTHORS COPYING INSTALL NEWS README ROADMAP

%changelog
* Tue Mar 19 2024 Daniel Gillen <development@sits.lu> 1.1.0-1
* Release 1.1.0-1
  See NEWS for details
— build package
