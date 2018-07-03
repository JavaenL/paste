# xpaste
A Windows utility that simply dumps the clipboard data to stdout.

Tiny yet safe, `xpaste` includes format checks and has zero dependencies.

Usage: `xpaste --[cr]lf` (default to keep the original line ending).

# xclip
A Windows utility that copy text from stdin or command line arguments to the clipboard.

Tiny yet safe, `xclip` includes charset checks and has zero dependencies, including on the standard library.

Usage (default to think stdin UTF-8 encoded):
```
x-clip: [-a|--ansi[i] | -u|--ucs2|--unicode] <<< DATA
        [--] string to copy
```

Examples:
``` bash
$ xclip < utf8.txt # tail line endings are always trimmed
$ cat uincode-list.txt | x-clip -u
$ xclip -- here is a line to copy
$ xclip here is another line to copy
$ xclip --help # equals with empty arguments
```

# license
It's made by Dahan Gong and released under the MIT license.

The initial code of `xpaste` is from @neosmart.
