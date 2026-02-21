Name:		hellforge
Version:	1.0.0
Release:	1%{?dist}
Summary:	Level editor for idTech4-based games
Group:		Applications/Editors
License:	GPLv2 and LGPLv2 and BSD
URL:		https://github.com/klaussilveira/hellforge
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	cmake, desktop-file-utils

%description
 HellForge is a 3D level editor for idTech4-based games such as Doom 3,
 dhewm3 and RBDOOM-3-BFG, based on DarkRadiant.

%prep
%setup -q

%build
cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
desktop-file-install					\
  --dir=${RPM_BUILD_ROOT}%{_datadir}/applications	\
  ${RPM_BUILD_ROOT}%{_datadir}/applications/io.github.klaussilveira.hellforge.desktop

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README.md
%{_bindir}/*
%{_libdir}/hellforge/lib*
%{_libdir}/hellforge/modules
%{_libdir}/hellforge/plugins/eclasstree*
%{_datadir}/*

%package plugins-darkmod
Summary:	DarkMod-specific plugins for HellForge
Group:		Applications/Editors
Requires:	hellforge

%description plugins-darkmod
 These plugins are used for editing Dark Mod missions.

%files plugins-darkmod
%defattr(-,root,root,-)
%{_libdir}/hellforge/plugins/dm_*
