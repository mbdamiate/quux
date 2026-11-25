# Appendix — Input and Output

**Question:** Which input and output primitives do the reference programs use, and how do they
interact with cancellation?

**Status:** Partially open

**Decision:** D-09 (cancellation points), D-13 (errors), D-17 (streams), D-20 (local resources)

This appendix is not one of the 14 topics. It defines only what programs 06 and 10 need. Phase 4
replaces the blocking implementation with asynchronous I/O (kqueue and epoll) without changing
these semantics.

## Resources

1. Files, listeners and connections are **local resources** (`Local resource` in
   [data-model.md](../../specs/001-language-spec/data-model.md)). They cannot be sent to another
   node: sending one raises `:not-serializable` at the sender (section 13).
2. `(close! resource)` closes a resource. Closing twice does nothing. Reading from or writing to
   a closed resource raises `:resource-closed`.
3. A resource opened inside a scope is closed when that scope ends, whether it ends successfully
   or by failure.

## Primitives

| Primitive | Result | Waits? |
|---|---|---|
| `(open-file path)` | a file opened for reading | no |
| `(file-lines path)` | a stream of the lines of the file, without line terminators (section 10); the file is closed when the stream ends or is closed | while reading |
| `(read-line resource)` | the next line of a file or connection, or `nothing` at the end | yes |
| `(write-line connection text)` | writes `text` and a line terminator | yes |
| `(listen port)` | a listener on a local TCP port | no |
| `(accept listener)` | the next incoming connection | yes |
| `(connect host port)` | a connection to a listener | yes |
| `(print-line value)` | writes `(to-string value)` and a line terminator to standard output; strings are written without quotes | no |
| `(parse-integer text)` | the integer in `text` (section 02) | no |

4. Every primitive that waits is a **cancellation point** (section 07). A task cancelled while it
   waits in `read-line`, `write-line`, `accept` or `connect` raises `:cancelled`, and the
   resource stays usable by its scope's cleanup code.
5. When the other side of a connection closes it, `read-line` returns `nothing` once all data
   already sent has been read. Later reads keep returning `nothing`. Writing to a connection that
   the other side has closed raises `:resource-closed`.

## Examples

```quux
(with-scope [scope]
  (let [lines (file-lines "lines.txt")]
    (first lines)))                                     ;=> "1"   (for a numbered-lines file)

(with-scope [scope]
  (let [file (open-file "lines.txt")]
    (close! file)
    (read-line file)))                                  ;!! :resource-closed
```

## Errors

| Kind | When |
|---|---|
| `:resource-closed` | reading from or writing to a closed resource |
| `:cancelled` | the task is cancelled while a primitive waits |
| `:transport` | a connection fails for a network reason (for example, it is reset) |
| `:not-serializable` | a resource is sent to another node |

## Open points

- Files opened for writing, binary data and encodings other than UTF-8. **Open — closed in
  Phase 4.**
- Timeouts at the level of the operating system. **Open — closed in Phase 4.** Programs use
  `with-timeout` (section 08) for now.

## Used by programs

06 (`file-lines`, `parse-integer`, `print-line`), 10 (`listen`, `accept`, `connect`,
`read-line`, `write-line`, `close!`).
