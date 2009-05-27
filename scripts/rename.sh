#!/bin/sh
# 
# Walk a source tree and search/replace all occurences of a given term.
# The script comes handy for refactoring projects.
for i in $(find . -type f -name "*.c" -o -name "*.h"); do sed 's/search-term/replace-term/g' $i > $i-tmp; mv $i $i-backup; mv $i-tmp $i; done
