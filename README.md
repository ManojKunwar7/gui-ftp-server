# GUI FTP SERVER

## Credits
### TSODING (Used his libray) 
https://www.twitch.tv/tsoding

**String View:-**
https://github.com/tsoding/sv

**Nobuild:-**
https://github.com/tsoding/nobuild

### LIBCURL
**LIBCURL Library:-**
https://curl.se/libcurl/c/


## Features

- [x] Can browse ftp directory
- [x] Can download files from ftp server
- [x] Can Upload files to ftp-server


## USAGE

### Open a ftp server

```sh
$ gcc -o nobuild nouild.c
$ ./nobuild
```

### Download a file from ftp server

**Navigate to the file (press enter)**
**Only file can be downloaded!**


### Upload a file to ftp server (only if folder exists)

```sh
$ gcc -o nobuild nouild.c
$ ./nobuild upload [file_name] [destination_folder] // "folder/subfolder/*" 
```