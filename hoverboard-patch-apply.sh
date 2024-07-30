# clear local unstaged changes
cd hoverboard-firmware-hack-FOC
git checkout .
git apply --reject --whitespace=fix ../hoverboard.patch
