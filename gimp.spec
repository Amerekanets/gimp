%define subver   1.1
%define microver 19
%define ver      %{subver}.%{microver}
%define prefix	 /usr

Summary: The GNU Image Manipulation Program
Name: 		gimp
Version: 	%{ver}
Release: 	1
Copyright: 	GPL, LGPL
Group: 		Applications/Graphics
URL: 		http://www.gimp.org/
BuildRoot: 	%{_tmppath}/%{name}-%{version}-root
Docdir:		%{prefix}/doc
Prefix:		%{prefix}
Obsoletes: 	gimp-data-min
Obsoletes:	gimp-libgimp
Requires: 	gtk+ >= 1.2.0
Source: 	ftp://ftp.gimp.org/pub/gimp/unstable/v%{version}/%{name}-%{version}.tar.bz2

%description
The GIMP (GNU Image Manipulation Program) is a powerful image
composition and editing program, which can be extremely useful for
creating logos and other graphics for Web pages.  The GIMP has many of
the tools and filters you would expect to find in similar commercial
offerings, and some interesting extras as well. The GIMP provides a
large image manipulation toolbox, including channel operations and
layers, effects, sub-pixel imaging and anti-aliasing, and conversions,
all with multi-level undo.

The GIMP includes a scripting facility, but many of the included
scripts rely on fonts that we cannot distribute.  The GIMP FTP site
has a package of fonts that you can install by yourself, which
includes all the fonts needed to run the included scripts.  Some of
the fonts have unusual licensing requirements; all the licenses are
documented in the package.  Get
ftp://ftp.gimp.org/pub/gimp/fonts/freefonts-0.10.tar.gz and
ftp://ftp.gimp.org/pub/gimp/fonts/sharefonts-0.10.tar.gz if you are so
inclined.  Alternatively, choose fonts which exist on your system
before running the scripts.

Install the GIMP if you need a powerful image manipulation
program. You may also want to install other GIMP packages:
gimp-libgimp if you're going to use any GIMP plug-ins and
gimp-data-extras, which includes various extra files for the GIMP.

%package devel
Summary: GIMP plugin and extension development kit
Group: 		Applications/Graphics
Requires: 	gtk+-devel
%description devel
The gimp-devel package contains the static libraries and header files
for writing GNU Image Manipulation Program (GIMP) plug-ins and
extensions.

Install gimp-devel if you're going to create plug-ins and/or
extensions for the GIMP.  You'll also need to install gimp-limpgimp
and gimp, and you may want to install gimp-data-extras.

%package perl
Summary: GIMP perl extensions and plugins.
Group:		Applications/Graphics
Requires:	gimp
Requires:	perl

%description perl
The gimp-perl package contains all the perl extensions and perl plugins. 

%prep
%setup -q

%build
%ifarch alpha
MYARCH_FLAGS="--host=alpha-redhat-linux"
%endif

if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --quiet $MYARCH_FLAGS --prefix=%{prefix}
else
  CFLAGS="$RPM_OPT_FLAGS" %configure --quiet
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{prefix}/info $RPM_BUILD_ROOT/%{prefix}/include \
	$RPM_BUILD_ROOT/%{prefix}/lib $RPM_BUILD_ROOT/%{prefix}/bin
make prefix=$RPM_BUILD_ROOT/%{prefix} PREFIX=$RPM_BUILD_ROOT/%{prefix} install

# Strip the executables
strip $RPM_BUILD_ROOT/%{prefix}/bin/gimp
# Only strip execuable files and leave scripts alone.
strip `file $RPM_BUILD_ROOT/%{prefix}/lib/gimp/%{subver}/plug-ins/* | grep ELF | cut -d':' -f 1`

# Compress down the online documentation.
if [ -d $RPM_BUILD_ROOT/%{prefix}/man ]; then
  find $RPM_BUILD_ROOT/%{prefix}/man -type f -exec gzip -9nf {} \;
fi

#
# This perl madness will drive me batty
#
eval perl '-V:archname'
find $RPM_BUILD_ROOT/%{prefix}/lib/perl5 -type f -print | sed "s@^$RPM_BUILD_ROOT@@g" | grep -v perllocal.pod > gimp-perl

#
# Plugins and modules change often (grab the executeable ones)
#
echo "%defattr (0555, bin, bin)" > gimp-plugin-files
find $RPM_BUILD_ROOT/%{prefix}/lib/gimp/%{subver} -type f -exec file {} \; | grep -v perl | cut -d':' -f 1 | sed "s@^$RPM_BUILD_ROOT@@g"  >>gimp-plugin-files

#
# Now pull the perl ones out.
#
echo "%defattr (0555, bin, bin)" > gimp-perl-plugin-files
echo "%dir %{prefix}/lib/gimp/%{subver}/plug-ins" >> gimp-perl-plugin-files
find $RPM_BUILD_ROOT/%{prefix}/lib/gimp/%{subver} -type f -exec file {} \; | grep perl | cut -d':' -f 1 | sed "s@^$RPM_BUILD_ROOT@@g" >>gimp-perl-plugin-files

#
# Auto detect the lang files.
#
if [ -f /usr/lib/rpm/find-lang.sh ] ; then
 /usr/lib/rpm/find-lang.sh $RPM_BUILD_ROOT %{name}
 /usr/lib/rpm/find-lang.sh $RPM_BUILD_ROOT gimp-std-plugins
 /usr/lib/rpm/find-lang.sh $RPM_BUILD_ROOT gimp-script-fu
 cat %{name}.lang gimp-std-plugins.lang gimp-script-fu.lang \
    | sed "s:(644, root, root, 755):(444, bin, bin, 555):" > gimp-all.lang
fi

#
# Tips
#
echo "%defattr (444, bin, bin, 555)" >gimp-tips-files
echo "%{prefix}/share/gimp/%{subver}/tips/gimp_tips.txt" >> gimp-tips-files
for I in `ls $RPM_BUILD_ROOT/%{prefix}/share/gimp/%{subver}/tips/gimp*.[a-z]*.txt | sed "s@^$RPM_BUILD_ROOT/@@g"`; do
   tip_lang=`basename $I | cut -d'.' -f2`
   echo "%lang($tip_lang)    $I" >> gimp-tips-files
done

#
# Build the master filelists generated from the above mess.
#
cat gimp-plugin-files gimp-all.lang gimp-tips-files > gimp.files
cat gimp-perl gimp-perl-plugin-files > gimp-perl-files

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f gimp.files
%attr (0555, bin, man) %doc AUTHORS COPYING ChangeLog MAINTAINERS NEWS README TODO
%attr (0555, bin, man) %doc docs/*.txt docs/*.eps ABOUT-NLS README.i18n README.perl README.win32 TODO
%defattr (0444, bin, bin, 0555)
%dir %{prefix}/share/gimp/%{subver}
%dir %{prefix}/share/gimp/%{subver}/tips
%dir %{prefix}/lib/gimp/%{subver}
%dir %{prefix}/lib/gimp/%{subver}/modules
%dir %{prefix}/lib/gimp/%{subver}/plug-ins

%{prefix}/share/gimp/%{subver}/brushes/
%{prefix}/share/gimp/%{subver}/fractalexplorer/
%{prefix}/share/gimp/%{subver}/gfig/
%{prefix}/share/gimp/%{subver}/gflare/
%{prefix}/share/gimp/%{subver}/gimpressionist/
%{prefix}/share/gimp/%{subver}/gradients/
%{prefix}/share/gimp/%{subver}/help/
%{prefix}/share/gimp/%{subver}/palettes/
%{prefix}/share/gimp/%{subver}/patterns/
%{prefix}/share/gimp/%{subver}/scripts/

%{prefix}/share/gimp/%{subver}/gimprc
%{prefix}/share/gimp/%{subver}/gimprc_user
%{prefix}/share/gimp/%{subver}/gtkrc
%{prefix}/share/gimp/%{subver}/unitrc
%{prefix}/share/gimp/%{subver}/gimp_logo.ppm
%{prefix}/share/gimp/%{subver}/gimp_splash.ppm
%{prefix}/share/gimp/%{subver}/ps-menurc

%defattr (0555, bin, bin)
%{prefix}/share/gimp/%{subver}/user_install

%{prefix}/lib/libgimp-%{subver}.so.%{microver}.0.0
%{prefix}/lib/libgimp-%{subver}.so.%{microver}
%{prefix}/lib/libgimpui-%{subver}.so.%{microver}.0.0
%{prefix}/lib/libgimpui-%{subver}.so.%{microver}
%{prefix}/lib/libgck-%{subver}.so.%{microver}.0.0
%{prefix}/lib/libgck-%{subver}.so.%{microver}

%{prefix}/bin/gimp
%{prefix}/bin/embedxpm
%{prefix}/bin/gimpdoc
%{prefix}/bin/xcftopnm

%defattr (0444, bin, man)
%{prefix}/man/man1/gimp.1*
%{prefix}/man/man5/gimprc.5*

%files devel
%defattr (0555, bin, bin, 0555)
%{prefix}/bin/gimptool
%{prefix}/bin/gimp-config
%{prefix}/lib/*.so
%{prefix}/lib/*.la
%dir %{prefix}/lib/gimp/%{subver}
%dir %{prefix}/lib/gimp/%{subver}/modules
%{prefix}/lib/gimp/%{subver}/modules/*.la

%defattr (0444, root, root, 0555)
%{prefix}/share/aclocal/gimp.m4

%{prefix}/lib/*.a
%{prefix}/lib/gimp/%{subver}/modules/*.a

%{prefix}/include/libgimp/
%{prefix}/include/gck/
%{prefix}/man/man1/gimptool.1*

%files perl -f gimp-perl-files

%changelog
* Fri Apr 14 2000 Matt Wilson <msw@redhat.com>
- include subdirs in the help find
- remove gimp-help-files generation
- both gimp and gimp-perl own prefix/lib/gimp/1.1/plug-ins
- both gimp and gimp-devel own prefix/lib/gimp/1.1 and
  prefix/lib/gimp/1.1/modules

* Thu Apr 13 2000 Matt Wilson <msw@redhat.com>
- 1.1.19
- get all .mo files

* Wed Jan 19 2000 Gregory McLean <gregm@comstar.net>
- Version 1.1.15

* Wed Dec 22 1999 Gregory McLean <gregm@comstar.net>
- Version 1.1.14
- Added some auto %files section generation scriptlets


