# Redis challenge
This is a an attempt at a C++ implementation of the ["Build Your Own Redis" Challenge](https://codecrafters.io/challenges/redis).

# Instructions

## Installation/prerequisites

- A pretty recent C++ compiler that supports C++23 is required (what worked for me was g++ version 13 or newer). You need to have your system's default compiler point to this newer compiler (usually `/usr/bin/c++`) if you want vscode or whatever setup you have to work out of the box.
- cmake (3.29+) and make (4.2.1+)
- gtest is pulled in from a zip file (see the `CMakeLists.txt` file).
- `CLI11` (header-only lib) is included as a git submodule in this repository.

## codecrafters CLI
The codecrafters CLI is primarily how the program is tested. You can also use the `spawn_redis_server.sh` script to build and run the main executable called `server`.

## Faster iteration

- `make test` should run the unit tests I've written so far.
- There's also a `clean` rule to clean the build directory (e.g. `make clean` or even `make clean test`).
- `make run -- <any args go here>` if you want to run the `server` executable with flags.
- NOTE: you can't combine multiple rules (like `make clean run`) if you also try to pass arguments.

# Bugs / Missing Features / TODOs

- [ ] the code that reads from the client socket can only handle 1024 bytes.
- [ ] can't handle client messages with newlines (?)
- [ ] GET commands don't support multiple args or wildcards.
- [ ] set up `*san` builds and run them regularly.

# Progress Log

## Basic Parser Finished
I finished the base set of challenges using a really hacky/dirty parser

## Parser rehaul
I'm doing a rehaul of the parser.

- [x] basic unit tests for parser (can't rely on codecrafters test suite since it doesn't allow testing efactors). Start with roundtripping Messages.
- [x] hook up the new refactored parser parts to the Server and see if it works.
- [x] get IDE autocomplete to work in tests (having trouble with gtest)
- [x] fix roundtripping tests
- [x] turn on compiler warning flags
- [x] consider using istringstream instead of using iterators on the chars (also fix the bugs where we dereference end iterators).
- [ ] consider using the visitor pattern on the variant of Message data field (right now I use a bunch of switch statements).
- [x] clean up the header/source files. Lots of redundant stuff, stale comments, etc. Throw out old parser logic once the new refactored parser is hooked up.
- [x] cleanly break apart the server logic from the Parser file. Also clean up all the namespacing mess I made.
- [x] remove unnecessary header includes