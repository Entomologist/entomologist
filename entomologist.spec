#
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

# norootforbuild
Url:    http://entomologist.sourceforge.net
Name:           entomologist
BuildRequires:  gcc-c++

%if 0%{?fedora_version}
    %define breq qt4-devel
    %define qmake /usr/bin/qmake-qt4
    %define lrelease /usr/bin/lrelease-qt4
%endif
%if 0%{?mandriva_version}
    %define breq libqt4-devel
    %define qmake /usr/lib/qt4/bin/qmake
    %define lrelease /usr/lib/qt4/bin/lrelease
%endif
%if 0%{?suse_version}
    %define breq libqt4-devel update-desktop-files
    %define qmake /usr/bin/qmake
    %define lrelease /usr/bin/lrelease
%endif

Version:	0.5
Release:	0
Summary:	Open-source bug tracking on the desktop
Group:		Productivity/Other
License:	GPL v2
BuildRequires: %{breq}
Requires: sqlite3 libqt4-sql-sqlite
Source:		%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
AutoReqProv:    on

%description
A desktop client for monitoring bugs across multiple bug trackers (bugzilla, launchpad, trac, etc).

Authors:
--------
    Matt Barringer <mbarringer@suse.de> 

%prep
%setup

%build
%{qmake} PREFIX=$RPM_BUILD_ROOT/%{_prefix} Entomologist.pro
make buildroot=$RPM_BUILD_ROOT CFLAGS="$RPM_OPT_FLAGS"

%install
install -d $RPM_BUILD_ROOT/usr/bin
install -d $RPM_BUILD_ROOT/usr/share/entomologist
install -m 755 -p entomologist $RPM_BUILD_ROOT/%{_bindir}
make install
%if 0%{?suse_version}
    %suse_update_desktop_file entomologist
%endif

%clean
rm -rf $RPM_BUILD_ROOT

#%post
#%postun

%files
%defattr(-,root,root)
%dir %{_prefix}/share/entomologist
%doc README INSTALL
%{_bindir}/entomologist
%{_prefix}/share/entomologist/*.qm
%{_prefix}/share/applications/entomologist.desktop
%dir %{_datadir}/icons/hicolor/*/apps/
%dir %{_datadir}/icons/hicolor/*/
%dir %{_datadir}/icons/hicolor/
%{_datadir}/icons/hicolor/*/apps/entomologist.*
%doc COPYING

%changelog

