# LFGUtils
Command-line utilities to create and expand old LucasFilm Game (LFG) archives.  These archive files had extensions like .XXX, .ND3, .ND4, and .MI2 and were used in 1992-era LucasFilm PC games that came on disk but needed to be installed to a hard drive.

## LFGDump
```
Usage: LFGDump [options] archivefile
Extracts files from "LFG" archives used in older LucasFilm games.

Options:
   -d              Display process details
   -f              Force overwrite of existing files during extraction
   -i              Show archive info only (do not extract)
   -l              List output files
   -o output_dir   Extract to directory 'output_dir'
   -s              Display file stats
   -v              Display version info
```

## LFGMake
```
Usage: LFGMake [options] archive_name archive_file_1 archive_file_2 ...
Creates an LFG-type archive.

Options:
  -f filelist           Use filelist (text file) as archive file list
  -h                    Display this help
  -m initial_size size  Set max size for first and subsequent archive files
  -o optimize level     0-5 (0 is fast; 1,3 look ahead versions; 5 find best)
  -s                    Print stats
  -t                    Use ASCII (text) mode encoding of literals
  -v                    Print version info
  -w N                  Force sliding window size of N k (where N=1,2,4)
```

### Compilation
Both utilities can be built with Borland C using DoxBox (or an actual ancient DOS PC) using `LFGBUILD.BAT`. Note that 'make' and 'bcc' must be accessible (via path settings).  Borland C++ project files are also in the BC_PRJ subdirectory.

For Linux, use `make lfgdump lfgmake clean`.

For Windows using VisualStudio, a solution file is in the VS_PRJ directory.
