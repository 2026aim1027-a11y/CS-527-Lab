#ifndef COMPILER_H
#define COMPILER_H

/* Compiles srcfile into outfile, in the program.byte format (4 hex
 * bytes per line). If srcfile already looks like compiled bytecode
 * (every non-empty line is 4 space-separated hex bytes) it is copied
 * through unchanged instead of being re-parsed as assembly - this lets
 * the same executable accept either an assembly source file or an
 * already-compiled program.byte on the command line. */
void compile(const char *srcfile, const char *outfile);

#endif
