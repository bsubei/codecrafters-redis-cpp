# Redis

## Basic Parser Finished
I finished the base set of challenges using a really hacky/dirty parser

## Parser rehaul
I'm doing a rehaul of the parser.

- [ ] basic unit tests for parser (can't rely on codecrafters test suite since it doesn't allow testinrg efactors). Start with roundtripping Messages.
- [ ] hook up the new refactored parser parts to the Server and see if it works.
- [ ] consider using istringstream instead of using iterators on the chars (also fix the bugs where we dereference end iterators).
- [ ] consider using the visitor pattern on the variant of Message data field (right now I use a bunch of switch statements).
- [ ] clean up the header/source files. Lots of redundant stuff, stale comments, etc. Throw out old parser logic once the new refactored parser is hooked up.
- [ ] cleanly break apart the server logic from the Parser file. Also clean up all the namespacing mess I made.