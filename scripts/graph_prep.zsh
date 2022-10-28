#!/usr/bin/zsh

alias cwk="awk -F, -v OFS=','"

sed -e 's/PAR EHP/PAR_EHP/g' $1 |
cwk '($1 !~ /^[a-zA-Z]/ && $2 == "cycles") || $1 ~ /^[a-zA-Z]/' |
paste -d"," - - |
cwk '{print $2, $1, $4}' |
tail -n 8
