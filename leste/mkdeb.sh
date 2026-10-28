# In the arm64 image: work/pkg/<package>'s Depends from the libraries its
# ELF files need (the image's packages that have them), then the .deb
set -eu
pkg=$1 version=$2 d=/work/pkg/$1
deps=$(find "$d" -type f ! -path "$d/DEBIAN/*" -exec sh -c \
    'for f; do file -b "$f" | grep -q ELF && objdump -p "$f" | awk "/NEEDED/ {print \$2}"; done' sh {} + |
    sort -u | while read -r so; do
	# a library of the package itself (or one built here before it)
	[ -n "$(find "$d" -name "$so" -print -quit)" ] && continue
	p=$(dpkg -S "*/$so" 2> /dev/null | head -1 | cut -d: -f1) || true
	[ -n "$p" ] && echo "$p"
    done | sort -u | paste -sd, - | sed 's/,/, /g')
# no ELF files (headers, QML, data): only the Depends given, if any
sed -i "s/@SHLIBS@/$deps/; s/^Depends: , /Depends: /; /^Depends: *$/d" "$d/DEBIAN/control"
dpkg-deb --root-owner-group -Zxz --build "$d" "/work/out/${pkg}_${version}_arm64.deb" > /dev/null
dpkg-deb -I "/work/out/${pkg}_${version}_arm64.deb" | sed -n '/Depends/p'
