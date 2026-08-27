# jretrace — JVM runtime agent

Pure-Java supervisor protocol client for TODO.beyond-libc/04 P1.
It speaks RTRD framing over Unix domain sockets and emits
`source=runtime` events (`jvm.file.read`, `jvm.socket.create`, direct
`emit(...)`) into the same retraced journal as libc/kernel/Python lanes.

This is intentionally dependency-free: Java 16+ for Unix domain sockets,
no JNI, no external JSON library.
