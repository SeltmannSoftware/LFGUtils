# LFGUtils
Command-line utilities to create and expand old LucasFilm Game (LFG) archives.  These archive files had extensions like .XXX, .ND3, .ND4, and .MI2 and were used in 1992-era LucasFilm PC games that came on disk but needed to be installed to a hard drive.

LFGDump - Extract from archive.

LFGMake - Create archive.



Linux:

make lfgdump lfgmake clean

DOS:

Old-school users can compile DOS version; it has been tested using Borland C++ 3.1 (code is actually just c).
Use DoxBox (or an actual ancient DOS PC). Note that 'make' and 'bcc' should be accessible (ie, in path). Use:

LFGBUILD.BAT

or load the Borland project files in the BC_PRJ subdirectory.

For Windows using VisualStudio, solution file is in the VS_PRJ directory.
