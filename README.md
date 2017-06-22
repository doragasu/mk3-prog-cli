# Mojo-NES MK3 programmer Command Line Interface
Command Line Interface for the Awesome Mojo-NES MKIII programmer.

This utility allows to manage mojo-nes-mk3 cartridges, using a mojo-nes-mk3 programmer. The utility allows to program and read flash and RAM chips. A driver system allows to support several mapper chip implementations.


# Building
You will need a working GNU GCC compiler and an Awesome Mojo-NES MKIII programmer to burn the ROM to a MegaWiFi cartridge. You will also need to install `libftdi` and `libmpsse` libraries, including development headers. Once you have your development environment properly installed, the makefile should do all the hard work for you. Just browse the Makefile to suit it to your dev environment and type:
```
$ make
```
The mk3-prog program should be built, sitting in the working directory, ready to use.

# Usage
Once you have plugged a Mojo-NES MKIII cartridge into an Awesome Mojo-NES MKIII Programmer, you can use mk3-prog. The command line application invocation must be as follows:
```
$ mk3-prog [option1 [option1_arg]] […] [optionN [optionN_arg]]
```
The options (option1 ~ optionN) can be any combination of the ones listed below. Options must support short and long formats. Depending on the used option, and option argument (option_arg) must be supplied. Several options can be used on the same command, as long as the combination makes sense (e.g. it does make sense using the flash and verify options together, but using the help option with the flash option doesn't make too much sense).

| Option | Description |
|---|---|
| -f, --firm-ver | Get programmer firmware version |
| -c, --flash-chr <arg> | Flash file to CHR ROM |
| -p, --flash-prg <arg> | Flash file to PRG ROM |
| -C, --read-chr <arg> | Read CHR ROM to file |
| -P, --read-prg <arg> | Read PRG ROM to file |
| -e, --erase-chr | Erase CHR Flash |
| -E, --erase-prg | Erase PRG Flash |
| -s, --chr-sec-er <arg> | Erase CHR flash sector |
| -S, --prg-sec-er <arg> | Erase PRG flash sector |
| -V, --verify | Verify flash after writing file |
| -i, --flash-id | Obtain flash chips identifiers |
| -R, --read-ram <arg> | Read data from RAM chip |
| -W, --write-ram <arg> | Write data to RAM chip |
| -b, --fpga-flash <arg> | Upload bitfile to FPGA, using .xcf file |
| -a, --cic-flash <arg> | AVR CIC firmware flash |
| -F, --firm-flash <arg> | Flash programmer firmware |
| -m, --mpsse-if <arg> | Set MPSSE interface number |
| -M, --mapper <arg> | Set mapper: 1-NOROM, 2-MMC3, 3-NFROM |
| -d, --dry-run | Dry run: don't actually do anything |
| -r, --version | Show program version |
| -v, --verbose | Show additional information |
| -h, --help | Print help screen and exit |

The <arg> text indicates that the option takes an input argument. For the options requiring an argument that represents a memory image file, or a memory address, the syntax is as follows:
* Memory image file: file_name[:start_addr[:length]]. Along with the file name, optional address and length fields can be added, separated by the colon (:) character, resulting in the following format:
* Address: Specifies an address related to the command (e.g. the address to which to flash a cartridge ROM or WiFi firmware blob).

Some examples of the command invocation and its arguments are:
* `$ mk3-prog -VeEc chr_rom_file -p prg_rom_file` → Erases entire cartridge (both CHR and PRG flash chips), flashes chr_rom_file to CHR flash, prg_rom_file to PRG flash, and verifies the writes.
* `$ mk3-prog --erase_chr -c chr_rom_file:0x1000` → Erases entire CHR flash chip and flashes contents of chr_rom_file to CHR flash, starting at address 0x1000.
* `$ mk3-prog -S 0x10000` → Erases PRG flash sector containing 0x100000 address.
* `$ mk3-prog -Vp prg_rom_file:0x10000:32768` → Flashes 32 KiB of prg_rom_file to address 0x10000, and verifies the operation.
* `$ mk3-prog --read_chr chr_rom_file::1048576` → Reads 1 MiB of the CHR flash chip, and writes it to chr_rom_file. Note that if you want to specify length but do not want to specify address, you have to use two colon characters before length. This way, missing address argument is interpreted as 0.

# Authors
This program has been written by doragasu.

# Contributions
Contributions are welcome. If you find a bug please open an issue, and if you have implemented a cool feature/improvement, please send a pull request.

# License
This program is provided with NO WARRANTY, under the [GPLv3 license](https://www.gnu.org/licenses/gpl-3.0.html).