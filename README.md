# myls

A small reimplementation of the Unix `ls` command written in C. It supports common flags, recursive directory listing, and custom error handling. The repository includes automated tests that compare output against the system `ls`.

Supported options:
- `-a` list all files, including hidden files
- `-l` long listing format
- `-R` recursive directory listing
- `-n` print the number of files that would be listed
- `--help` display usage information

BUILD:
Requires a POSIX system, `gcc` or `clang`, and `make`. Build the program by running `make`. This creates a local executable named `ls`.

RUN:
Execute the program with `./ls`. Example usage includes `./ls -l`, `./ls -a`, `./ls -R .`, and `./ls -alR /tmp`. This does not replace the system `ls`.

TESTS:
The test suite is written in Bats. It requires `bats`, a POSIX shell, and `sudo`. The setup script creates files with unknown user and group IDs using `chown`. Sudo is needed only for test setup and cleanup. Run tests with `make test`. Tests will fail without sudo access.
