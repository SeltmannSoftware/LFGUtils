  mkdir %4_
  del %4_\*
  lfgdump -s -f -o %4_ %1\%2.%3 >> %4.LOG
  dir /b %4_ > %4_.txt
  cd %4_
  lfgmake -f ..\%4_.txt -m 1454088 1456136 -s %2.LFG  >> ..\%4.LOG
  mkdir ..\_%4
  lfgdump -s -d -f -o ..\_%4 %2.LFG >> ..\%4.LOG
  cd ..

