@echo off
mkdir %4_
del /Q %4_\*
lfgdump -s -f -o %4_ %1\%2.%3
fc /B %5\* %4_\* 
lfgdump -i -l %1\%2.%3 > %4_.txt
cd %4_
lfgmake -f ..\%4_.txt -o 5 -m 1454088 1456136 -s ..\%2.LFG
cd ..
mkdir _%4
lfgdump -s -d -f -o _%4 %2.LFG 
fc /B _%4\* %4_\* 
