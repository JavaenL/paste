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

# Useful bash snippets

Th snippet below supports:
* copy using stdin/arguments & paste using stdout
* paste to a file by `paste a.txt`
* paste to a local variable by `paste -var foo`
* copy a file's content by `clip a.txt`

``` bash
function xpaste() {
  local arg=$1
  if test "$arg" == "-var" -a -n "$2"; then
    # paste from clipboard into a global bash variable
    declare -g "$2"="$(exec "$MSYS_ROOT"/usr/bin/xpaste.exe --lf)"
    return
  fi
  test "${arg#-}" != "$arg" && shift || arg=
  if test -n "$1"; then
    "$MSYS_ROOT"/usr/bin/xpaste.exe $arg >"$1"
  else
    "$MSYS_ROOT"/usr/bin/xpaste.exe $arg
  fi
}
function xclip() {
  local arg=$1
  if test -n "$arg" && test -f "$arg"; then
    shift
    "$MSYS_ROOT"/usr/bin/xclip.exe "$@" <"$arg"
  else
    "$MSYS_ROOT"/usr/bin/xclip.exe "$@"
  fi
}
alias paste=xpaste clip=xclip
```

# License
It's made by Dahan Gong and released under the MIT license.

The initial code of `xpaste` is from @neosmart.
