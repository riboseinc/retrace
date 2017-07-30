# Description

A 'stringinjector' tool written in C as part of retrace. For now it's a standalone tool but will be integrated in retrace at a later stage.

Stringinjector inserts strings and/or replaces bytes in ascii and binary files. As the files can be ascii in nature (XML, JSON etc) or binary (think ELF) it needs to be able to write both. It needs to be fast as some files can be several megabytes big.

The strings need to be injected without 0 termination or \n line break.

---

There are 5 main types of injection:

---

# Type 1

Take an existing file, go to 1 specific position (POS A) and insert a single hex byte at A (0x00), repeat this process 255 times until the last hex byte is reached (0xff), save each iteration as a new file, this will lead to 255 new files

argument: -h hex

file.ascii content: AAAABBBBCCCCDDDDEEEEFFFF

``` sh
         CMD   INFILE     OUT_TEMPLATE   ARG POS
command: ./cmd file.ascii file.ascii.out -h  5
```

generates 255 files where each file has the 5th character replaced with a single hex 0x00-0xff:

``` sh
file.ascii.out.0
file.ascii.out.1
file.ascii.out.2
file.ascii.out.n
file.ascii.out.255
```

contents:

``` sh
file.ascii.out.0
00000000 41 41 42 42 00 43 44 44 45 45 46 46 0a |AABB.CDDEEFF.|
file.ascii.out.1
00000000 41 41 42 42 01 43 44 44 45 45 46 46 0a |AABB.CDDEEFF.|
file.ascii.out.2
00000000 41 41 42 42 02 43 44 44 45 45 46 46 0a |AABB.CDDEEFF.|
file.ascii.out.3
00000000 41 41 42 42 03 43 44 44 45 45 46 46 0a |AABB.CDDEEFF.|
file.ascii.out.255
00000000 41 41 42 42 ff 43 44 44 45 45 46 46 0a |AABB.CDDEEFF.|
```

---

# Type 2

Take an existing file, go to 2 specific positions (POS A and POS B) and insert a single hex byte at POS A and POS B, repeat this process 255 times until the last hex byte is reached (0xff), increase POS A with 1 while POS B is reset to 0 and repeats the iteration until 0xff, save each iteration as a new file, this will lead to 65025 (255 * 255) new files

argument: -h hex

file.ascii content: AAAABBBBCCCCDDDDEEEEFFFF

``` sh
         CMD   INFILE     OUT_TEMPLATE   ARG POS
command: ./cmd file.ascii file.ascii.out -h  5,7
```

generates 65025 files where each file has the 5th character replaced with a single hex 0x00-0xff and the 7th character replaced with a single hex 0x00-0xff:

``` sh
file.ascii.out.0.0
file.ascii.out.0.1
file.ascii.out.0.2
file.ascii.out.0.n
file.ascii.out.0.255
file.ascii.out.1.0
file.ascii.out.1.1
file.ascii.out.1.2
file.ascii.out.1.n
file.ascii.out.255.255
```

contents:

``` sh
file.ascii.out.0.0
00000000 41 41 42 42 00 43 78 00 45 45 46 46 0a |AABB.C.DEEFF.|
file.ascii.out.0.1
00000000 41 41 42 42 00 43 78 01 45 45 46 46 0a |AABB.C.DEEFF.|
file.ascii.out.0.2
00000000 41 41 42 42 00 43 78 02 45 45 46 46 0a |AABB.C.DEEFF.|
file.ascii.out.255.255
00000000 41 41 42 42 ff 43 78 ff 45 45 46 46 0a |AABB.C.DEEFF.|
```

---

# Type 3

Take an existing file, go to 1 specific position (POS A) and generate a N (based on a specific length) byte format string '%s', this will lead to a new file

argument: -f format string

file.ascii content: AAAABBBBCCCCDDDDEEEEFFFF

``` sh
         CMD   INFILE     OUT_TEMPLATE   ARG POS COUNT
command: ./cmd file.ascii file.ascii.out -f  5   2
```

generates 1 file with a 4 byte format string (4 bytes = '%s%s' = count 2) inserted in the file 'file.ascii' at position 5

contents:

file.ascii.out.0

``` sh
00000000 41 41 42 42 25 73 25 73 43 43 44 44 45 45 46 46 | AABB%s%sCCDDEEFF|
```

remarks: count is the number of format specifiers injected

---

# Type 4

Take an existing file, go to 1 specific position (POS A) and generate a N (based on a specific length) byte string consisting of 'A' / '\x41', this will lead to a new file

argument: -o overflow

file.ascii content: AAAABBBBCCCCDDDDEEEEFFFF

``` sh
         CMD   INFILE     OUT_TEMPLATE   ARG POS LEN
command: ./cmd file.ascii file.ascii.out -o  5   2
```

generates 1 file with a 2 byte string consisting of 'AA' / '\x41\x41' inserted in the file 'file.ascii' at position 5

contents:

file.ascii.out.0

``` sh
00000000 41 41 42 42 41 41 41 4143 43 44 44 45 45 46 46|AABBAACCDDEEFF|
```

---

# Type 5

Take an existing file, go to 1 specific position (POS A) and insert a string from a file containing strings (read 1st line), save to a new file, then read line 2 from the strings file and insert this line into position POS A and save as a new file

argument:-i insert strings read in from a file

file.ascii content: AAAABBBBCCCCDDDDEEEEFFFF

``` sh
         CMD   INFILE     OUT_TEMPLATE   ARG POS FILE
command: ./cmd file.ascii file.ascii.out -i  5   file.insert.txt
```

The inserted file can have several lines ending with a linebreak \n:

``` sh
file.ascii.out.0 line 1
file.ascii.out.1 line 2
file.ascii.out.2 line 3
file.ascii.out.n line n
```
---

``` sh
Remarks: this functionality is to safely insert command injection strings e.g.: `id` $(id) etc without having to worry about the shell
executing these strings locally which would normally happen if you'd provide these strings as arguments to the program without escaping.
```
