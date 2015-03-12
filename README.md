## GstHantroG1 

GStreamer 1.x plug-in supporting the Hantro G1 HW accelerated decoder

## Integrating the Codecs 

Besides the regular configure options exhaustively described in the INSTALL 
guide of this project, the are some G1 codec specific integration options.

| Configure Option | Description |
|------------------|-------------|
| G1_CFLAGS | compiler flags for the G1 decoder |
| G1_LIBS | linker flags for the G1 decoder packge |
| --with-g1-dwl-path=PATH | Path to an alternative DWL library |
| --with-g1-h264-path=PATH | Path to an alternative H264 library |
| --with-g1-mpeg4-path=PATH | Path to an alternative MPEG4 library |
| --with-g1-jpeg-path=PATH | Path to an alternative JPEG library |
| --with-g1-vp8-path=PATH | Path to an alternative VP8 library |
| --with-g1-pp-path=PATH | Path to an alternative PP library |

### Example 

The following example shows the minimum configuration needed if 
the G1 header files and libraries are located on non-standard 
locations.

```bash
PKG_CONFIG_LIBDIR=$PATH_TO_PKG_CONFIG ./configure \
    --prefix=/usr/ \
    --build=x86_64-unknown-linux-gnu \
    --host=armv7l-timesys-linux-gnueabihf \
    G1_CFLAGS="-I${PATH_TO_G1_SOURCE}/g1_decoder/software/source/inc/" \
    G1_LIBS="-L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/dwl/ \
      -L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/h264high/ \
      -L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/mpeg4/ \
      -L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/jpeg/ \
      -L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/vp8/ \
      -L${PATH_TO_G1_SOURCE}/g1_decoder/software/linux/pp/"
```

where PATH_TO_PKG_CONFIG is the location of the *.pc files in the 
development environment and PATH_TO_G1_SOUCE is the location of the 
the root G1 source files.