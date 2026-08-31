package org.retrace.runtime;

import java.io.Closeable;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

/**
 * jretrace -- the JVM runtime agent (TODO.beyond-libc/04 P1).
 *
 * A pure-Java implementation of the retrace supervisor protocol:
 * RTRD framing over UDS, HELLO/HEARTBEAT/EVENT/BYE, source=runtime.
 * It is intentionally tiny so JVM services can embed it without a
 * native dependency; the second runtime proves the supervisor seam.
 */
public final class JRetrace implements Closeable {
    private static final byte[] MAGIC = new byte[] {'R', 'T', 'R', 'D'};
    private static final int HELLO = 1;
    private static final int HEARTBEAT = 2;
    private static final int EVENT = 4;
    private static final int BYE = 6;
    private static final int WELCOME = 16;

    private final Chan channel;
    private final String agentId;
    private final AtomicLong seq = new AtomicLong();
    private final AtomicBoolean stop = new AtomicBoolean();
    private final Thread heartbeat;

    private JRetrace(Chan channel, String agentId) {
        this.channel = channel;
        this.agentId = agentId;
        this.heartbeat = new Thread(this::heartbeatLoop, "jretrace-heartbeat");
        this.heartbeat.setDaemon(true);
        this.heartbeat.start();
    }

    /* Channel seam: UDS (SocketChannel) on POSIX, the byte-mode
     * named pipe (RandomAccessFile) on Windows -- Java has no
     * AF_UNIX there, but a pipe opens as a file and speaks the
     * same RTRD byte framing. */
    private interface Chan extends Closeable {
        void writeAll(byte[] buf, int n) throws IOException;

        byte[] readExact(int n) throws IOException;
    }

    private static final class UdsChan implements Chan {
        private final SocketChannel ch;

        UdsChan(SocketChannel ch) {
            this.ch = ch;
        }

        public void writeAll(byte[] buf, int n) throws IOException {
            java.nio.ByteBuffer b = java.nio.ByteBuffer.wrap(buf, 0, n);
            while (b.hasRemaining())
                ch.write(b);
        }

        public byte[] readExact(int n) throws IOException {
            java.nio.ByteBuffer b = java.nio.ByteBuffer.allocate(n);
            while (b.hasRemaining()) {
                if (ch.read(b) < 0)
                    throw new IOException("eof");
            }
            return b.array();
        }

        public void close() throws IOException {
            ch.close();
        }
    }

    private static final class PipeChan implements Chan {
        private final RandomAccessFile f;

        PipeChan(String path) throws IOException {
            /*
             * The daemon keeps one pending instance; a connect
             * landing in the gap after it is consumed (and
             * before its replacement exists) is refused. Retry
             * briefly -- the same backoff the in-process agent
             * carries.
             */
            RandomAccessFile opened = null;
            IOException last = null;
            for (int i = 0; i < 5 && opened == null; i++) {
                try {
                    opened = new RandomAccessFile(path, "rw");
                } catch (IOException e) {
                    last = e;
                    try {
                        Thread.sleep(200);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            }
            if (opened == null)
                throw last;
            f = opened;
        }

        public void writeAll(byte[] buf, int n) throws IOException {
            f.write(buf, 0, n);
        }

        public byte[] readExact(int n) throws IOException {
            byte[] buf = new byte[n];
            int off = 0;
            while (off < n) {
                int got = f.read(buf, off, n - off);
                if (got <= 0)
                    throw new IOException("eof");
                off += got;
            }
            return buf;
        }

        public void close() throws IOException {
            f.close();
        }
    }

    public static JRetrace supervise() throws IOException {
        if (!"1".equals(System.getenv("RETRACE_SUPERVISOR"))) {
            return null;
        }
        String sock = System.getenv("RETRACE_SUPERVISOR_SOCK");
        if (sock == null || sock.isEmpty()) {
            return null;
        }
        String nonce = System.getenv("RETRACE_SUPERVISOR_NONCE");
        if (nonce == null) {
            nonce = "";
        }
        Chan ch;
        if (sock.startsWith("\\\\\\\\.\\\\pipe\\\\")) {
            ch = new PipeChan(sock);
        } else {
            SocketChannel uds =
                SocketChannel.open(StandardProtocolFamily.UNIX);
            uds.connect(UnixDomainSocketAddress.of(sock));
            ch = new UdsChan(uds);
        }
        long pid = ProcessHandle.current().pid();
        long ppid = ProcessHandle.current().parent()
            .map(ProcessHandle::pid).orElse(0L);
        Map<String, String> hello = new LinkedHashMap<>();
        hello.put("session_token", getenv("RETRACE_SESSION"));
        hello.put("nonce", nonce);
        hello.put("pid", Long.toString(pid));
        hello.put("ppid", Long.toString(ppid));
        hello.put("boot_id", "jvmruntime");
        hello.put("cmdline", System.getProperty("sun.java.command", "java"));
        hello.put("retrace_version", "jretrace-1");
        writeFrame(ch, HELLO, jsonObject(hello));
        Frame welcome = readFrame(ch);
        if (welcome.mid != WELCOME) {
            ch.close();
            throw new IOException("expected WELCOME, got " + welcome.mid);
        }
        String agent = jsonString(welcome.payload, "agent_id");
        if (agent == null || agent.isEmpty()) {
            agent = "pending";
        }
        /* UDS only: peek a possible POLICY_SET without blocking.
         * Pipes block, but the agent runs full-role -- the daemon
         * pushes policy immediately if configured, so a blocking
         * read is CORRECT there (it arrives or the daemon holds).*/
        if (ch instanceof UdsChan) {
            SocketChannel uds = ((UdsChan) ch).ch;
            uds.configureBlocking(false);
            drainPolicy(ch);
            uds.configureBlocking(true);
        }
        return new JRetrace(ch, agent);
    }

    public void emit(String name, Map<String, String> attrs) throws IOException {
        long next = seq.incrementAndGet();
        Map<String, String> root = new LinkedHashMap<>();
        root.put("agent_id", agentId);
        root.put("seq", Long.toString(next));
        root.put("ts", Long.toString(Instant.now().getEpochSecond()));
        root.put("name", name);
        root.put("source", "runtime");
        synchronized (channel) {
            writeFrame(channel, EVENT, eventJson(root, attrs));
        }
    }

    public void fileRead(String path) throws IOException {
        Map<String, String> attrs = new LinkedHashMap<>();
        attrs.put("path", path);
        emit("jvm.file.read", attrs);
    }

    public void socketCreate(String family) throws IOException {
        Map<String, String> attrs = new LinkedHashMap<>();
        attrs.put("family", family);
        emit("jvm.socket.create", attrs);
    }

    @Override
    public void close() throws IOException {
        stop.set(true);
        try {
            heartbeat.join(1500);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        synchronized (channel) {
            writeFrame(channel, BYE, "{\"agent_id\":\"" + esc(agentId) + "\"}");
            channel.close();
        }
    }

    private void heartbeatLoop() {
        while (!stop.get()) {
            try {
                Thread.sleep(1000L);
                synchronized (channel) {
                    writeFrame(channel, HEARTBEAT,
                        "{\"agent_id\":\"" + esc(agentId) +
                        "\",\"seq\":" + seq.get() + "}");
                }
            } catch (IOException e) {
                stop.set(true);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                stop.set(true);
            }
        }
    }

    private static void drainPolicy(Chan ch) throws IOException {
        SocketChannel uds = ((UdsChan) ch).ch;
        ByteBuffer hdr = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN);
        int n = uds.read(hdr);
        if (n <= 0) {
            return;
        }
        while (hdr.hasRemaining()) {
            n = uds.read(hdr);
            if (n <= 0) {
                return;
            }
        }
        hdr.flip();
        hdr.position(8);
        int len = hdr.getInt();
        ByteBuffer body = ByteBuffer.allocate(Math.max(0, len));
        while (body.hasRemaining() && uds.read(body) > 0) {
            // drain only
        }
    }

    private static void writeFrame(Chan ch, int mid, String payload)
        throws IOException {
        byte[] b = payload.getBytes(StandardCharsets.UTF_8);
        ByteBuffer buf = ByteBuffer.allocate(12 + b.length)
            .order(ByteOrder.LITTLE_ENDIAN);
        buf.put(MAGIC);
        buf.putShort((short)1);
        buf.putShort((short)mid);
        buf.putInt(b.length);
        buf.put(b);
        buf.flip();
        ch.writeAll(buf.array(), buf.limit());
    }

    private static Frame readFrame(Chan ch) throws IOException {
        byte[] head = ch.readExact(12);
        if ((head[0] != 'R') || (head[1] != 'T') || (head[2] != 'R')
            || (head[3] != 'D')) {
            throw new IOException("bad magic");
        }
        int mid = Short.toUnsignedInt(
            (short) ((head[7] & 0xff) << 8 | (head[6] & 0xff)));
        int len = (head[8] & 0xff) | (head[9] & 0xff) << 8
            | (head[10] & 0xff) << 16 | (head[11] & 0xff) << 24;
        byte[] body = ch.readExact(len);
        return new Frame(mid, new String(body, StandardCharsets.UTF_8));
    }

    private static String jsonObject(Map<String, String> vals) {
        StringBuilder sb = new StringBuilder("{");
        boolean first = true;
        for (Map.Entry<String, String> e : vals.entrySet()) {
            if (!first) {
                sb.append(',');
            }
            first = false;
            sb.append('"').append(esc(e.getKey())).append("\":");
            if (isNumber(e.getValue())) {
                sb.append(e.getValue());
            } else {
                sb.append('"').append(esc(e.getValue())).append('"');
            }
        }
        sb.append('}');
        return sb.toString();
    }

    private static String eventJson(Map<String, String> root,
        Map<String, String> attrs) {
        StringBuilder sb = new StringBuilder(jsonObject(root));
        int insert = sb.length() - 1;
        StringBuilder a = new StringBuilder(",\"attrs\":{");
        boolean first = true;
        if (attrs != null) {
            for (Map.Entry<String, String> e : attrs.entrySet()) {
                if (!first) {
                    a.append(',');
                }
                first = false;
                a.append('"').append(esc(e.getKey())).append("\":\"")
                    .append(esc(e.getValue())).append('"');
            }
        }
        a.append('}');
        sb.insert(insert, a);
        return sb.toString();
    }

    private static boolean isNumber(String s) {
        if (s == null || s.isEmpty()) {
            return false;
        }
        for (int i = 0; i < s.length(); i++) {
            if (!Character.isDigit(s.charAt(i))) {
                return false;
            }
        }
        return true;
    }

    private static String getenv(String k) {
        String v = System.getenv(k);
        return v == null ? "" : v;
    }

    private static String jsonString(String json, String key) {
        String needle = "\"" + key + "\":\"";
        int i = json.indexOf(needle);
        if (i < 0) {
            return null;
        }
        i += needle.length();
        StringBuilder out = new StringBuilder();
        while (i < json.length()) {
            char c = json.charAt(i++);
            if (c == '"') {
                return out.toString();
            }
            if (c == '\\' && i < json.length()) {
                out.append(json.charAt(i++));
            } else {
                out.append(c);
            }
        }
        return null;
    }

    private static String esc(String s) {
        if (s == null) {
            return "";
        }
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
            case '\\':
                out.append("\\\\");
                break;
            case '"':
                out.append("\\\"");
                break;
            case '\n':
                out.append("\\n");
                break;
            case '\r':
                out.append("\\r");
                break;
            case '\t':
                out.append("\\t");
                break;
            default:
                if (c < 0x20) {
                    out.append(String.format("\\u%04x", (int)c));
                } else {
                    out.append(c);
                }
            }
        }
        return out.toString();
    }

    private record Frame(int mid, String payload) {}
}
