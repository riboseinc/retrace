package org.retrace.runtime;

import java.io.Closeable;
import java.io.IOException;
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

    private final SocketChannel channel;
    private final String agentId;
    private final AtomicLong seq = new AtomicLong();
    private final AtomicBoolean stop = new AtomicBoolean();
    private final Thread heartbeat;

    private JRetrace(SocketChannel channel, String agentId) {
        this.channel = channel;
        this.agentId = agentId;
        this.heartbeat = new Thread(this::heartbeatLoop, "jretrace-heartbeat");
        this.heartbeat.setDaemon(true);
        this.heartbeat.start();
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
        SocketChannel ch = SocketChannel.open(StandardProtocolFamily.UNIX);
        ch.connect(UnixDomainSocketAddress.of(sock));
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
        ch.configureBlocking(false);
        drainPolicy(ch);
        ch.configureBlocking(true);
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

    private static void drainPolicy(SocketChannel ch) throws IOException {
        ByteBuffer hdr = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN);
        int n = ch.read(hdr);
        if (n <= 0) {
            return;
        }
        while (hdr.hasRemaining()) {
            n = ch.read(hdr);
            if (n <= 0) {
                return;
            }
        }
        hdr.flip();
        hdr.position(8);
        int len = hdr.getInt();
        ByteBuffer body = ByteBuffer.allocate(Math.max(0, len));
        while (body.hasRemaining() && ch.read(body) > 0) {
            // drain only
        }
    }

    private static void writeFrame(SocketChannel ch, int mid, String payload)
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
        while (buf.hasRemaining()) {
            ch.write(buf);
        }
    }

    private static Frame readFrame(SocketChannel ch) throws IOException {
        ByteBuffer hdr = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN);
        while (hdr.hasRemaining()) {
            if (ch.read(hdr) < 0) {
                throw new IOException("eof");
            }
        }
        hdr.flip();
        byte[] magic = new byte[4];
        hdr.get(magic);
        if (magic[0] != 'R' || magic[1] != 'T' || magic[2] != 'R' ||
            magic[3] != 'D') {
            throw new IOException("bad magic");
        }
        hdr.getShort();
        int mid = Short.toUnsignedInt(hdr.getShort());
        int len = hdr.getInt();
        ByteBuffer body = ByteBuffer.allocate(len);
        while (body.hasRemaining()) {
            if (ch.read(body) < 0) {
                throw new IOException("eof");
            }
        }
        body.flip();
        return new Frame(mid,
            StandardCharsets.UTF_8.decode(body).toString());
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
