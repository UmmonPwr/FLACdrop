FLACdrop can convert between different audio file formats. You can drop the files on it to start the conversion.

Aim was to speed up the encoding process. The used audio libraries are designed as single thread in mind, so to utilize multithread capabilities it is launching independent encoder threads for each file.
FLACdrop can run maximum eight parallel threads. Maximum parallel thread number can be set in the options menu to adjust for actual CPU performance.

Currently it can convert:
- from WAV to MP3
- from WAV to FLAC
- from FLAC to WAV
- from FLAC to MP3

FLACdrop is using the below audio libraries. Only the headers and the pre-built lib files are included:
- libflac 1.3.4 GitHub version ( https://github.com/xiph/flac )
- libogg 1.3.3 GitHub version ( https://github.com/xiph/ogg )
- libmp3lame 3.100.2 ( http://lame.sourceforge.net/ )