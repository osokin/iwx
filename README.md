
# A port of OpenBSD iwx driver to FreeBSD

Based on https://www.freebsd.org/doc/en_US.ISO8859-1/books/arch-handbook/pci.html example.

--------------------------------------------------------------------------

#### HOWTO COMPILE AND INSTALL:

Place source in `/usr/src/sys/dev/iwx`

```
make clean
make
make install
```

#### HOWTO RUN:
```
kldload iwx
```

#### TODO:

 - **Porting!**
