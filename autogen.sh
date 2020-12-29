#! /bin/sh

set -e

aclocal
autoconf
autoheader

# FIXME: automake doesn't like that there's no Makefile.am
automake -a -c -f || true

echo timestamp > stamp-h.in

if python2 -c "import re,sys,os,datetime" 2>/dev/null >/dev/null ; then
  python2 util/import_gcry.py lib/libgcrypt/ .
fi

for rmk in conf/*.rmk ${GRUB_CONTRIB}/*/conf/*.rmk; do
  if test -e $rmk ; then
    ruby genmk.rb < $rmk > `echo $rmk | sed 's/\.rmk$/.mk/'`
  fi
done
sh gendistlist.sh > DISTLIST

exit 0
