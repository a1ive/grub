#!/bin/sh
mkdir -p PKG/lib/grub/fonts
cp -r makepkg/* PKG/lib/grub/
cp PKG/share/grub/*.pf2 PKG/lib/grub/fonts/
cd PKG/lib/

for x in $(ls grub/locale/*.po)
do
    po=$(echo "$x" | sed 's/\.[^.]*$//')
    msgfmt "$x" -o "$po.mo"
done

rm -f grub/locale/*.po
rm -f grub/*/*.module
rm -f grub/*/*.image
rm -f grub/*/*.exec
rm -f grub/*/config.h

tar -zcvf grub.tar.gz grub
mv grub.tar.gz ../../
