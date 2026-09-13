// Package retrace -- the Go runtime agent (TODO.impl/12).
//
// A THIRD-PARTY implementation of the retrace supervisor
// protocol (the same framing and state machine the conformance
// suite's reference stub and the Python/Node/JVM siblings
// walk). The cgo-free hook surface is the runtime's own
// boundaries: an http.RoundTripper wrapper names each request
// with the layer a libc interposer sees only as "a connect
// from pid N"; Emit carries anything the host program wants to
// attribute.
//
//	retrace.Supervise()          // joins RETRACE_SUPERVISOR env
//	retrace.HookHTTP()           // wraps http.DefaultTransport
//	retrace.Emit("go.marker", map[string]string{"k": "v"})
//
// Zero-mandatory-config: with the env absent, Supervise is a
// no-op (the preload plane's own gating doctrine). The agent
// registers as source=runtime; the labeler keeps libc / kernel
// / runtime as lanes of one session.
//
// Wire contract: RTRD framing, the frozen v1 message table
// (see src/supervisor/protocol.h; the conformance suite pins
// both sides to the same artifacts).
package retrace

import (
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

const (
	magic   = "RTRD"
	verV1   = 1
	mHello  = 1
	mBeat   = 2
	mEvent  = 4
	mBye    = 6
	mWelc   = 16
	beatDur = time.Second
)

type state struct {
	mu      sync.Mutex
	sock    string
	nonce   string
	conn    io.ReadWriteCloser
	agentID string
	seq     uint64
	stop    chan struct{}
	done    chan struct{}
	hooked  bool
}

var st state

// Supervise joins the supervisor named by RETRACE_SUPERVISOR;
// a no-op when absent. Returns whether the agent armed.
func Supervise() bool {
	if os.Getenv("RETRACE_SUPERVISOR") != "1" {
		return false
	}
	sock := os.Getenv("RETRACE_SUPERVISOR_SOCK")
	if sock == "" {
		return false
	}
	st.mu.Lock()
	if st.stop != nil {
		st.mu.Unlock()
		return true
	}
	st.sock = sock
	st.nonce = os.Getenv("RETRACE_SUPERVISOR_NONCE")
	st.stop = make(chan struct{})
	done := make(chan struct{})
	st.done = done
	st.mu.Unlock()
	go agentLoop()
	// settle past HELLO/WELCOME so early emits carry the id
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		st.mu.Lock()
		ready := st.conn != nil
		st.mu.Unlock()
		if ready {
			break
		}
		select {
		case <-done:
			return false
		case <-time.After(50 * time.Millisecond):
		}
	}
	return true
}

// Shutdown sends BYE and closes the connection (the graceful
// path; without it the daemon's liveness sweep reaps the seat).
func Shutdown() {
	st.mu.Lock()
	stop := st.stop
	st.mu.Unlock()
	if stop == nil {
		return
	}
	select {
	case <-stop:
	default:
		close(stop)
	}
	st.mu.Lock()
	done := st.done
	st.mu.Unlock()
	if done != nil {
		select {
		case <-done:
		case <-time.After(2 * time.Second):
		}
	}
}

// pipeConn adapts a byte-mode named pipe opened as a file to
// the socket surface the agent speaks (Windows Go has no
// AF_UNIX; the pipe carries the same RTRD framing).
type pipeConn struct {
	f *os.File
}

func (p pipeConn) Read(b []byte) (int, error)  { return p.f.Read(b) }
func (p pipeConn) Write(b []byte) (int, error) { return p.f.Write(b) }
func (p pipeConn) Close() error                { return p.f.Close() }

func dial(sock string) (io.ReadWriteCloser, error) {
	if strings.HasPrefix(sock, `\\.\pipe\`) {
		return pipeConn{}, nil // placeholder; replaced below
	}
	return net.Dial("unix", sock)
}

func dialPipe(sock string) (io.ReadWriteCloser, error) {
	f, err := os.OpenFile(sock, os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}
	return pipeConn{f: f}, nil
}

func frame(mid uint16, payload []byte) []byte {
	hdr := make([]byte, 12)
	copy(hdr, magic)
	binary.LittleEndian.PutUint16(hdr[4:], verV1)
	binary.LittleEndian.PutUint16(hdr[6:], mid)
	binary.LittleEndian.PutUint32(hdr[8:], uint32(len(payload)))
	return append(hdr, payload...)
}

func send(mid uint16, obj any) {
	st.mu.Lock()
	c := st.conn
	st.mu.Unlock()
	if c == nil {
		return
	}
	b, err := json.Marshal(obj)
	if err != nil {
		return
	}
	if _, err := c.Write(frame(mid, b)); err != nil {
		st.mu.Lock()
		if st.conn == c {
			c.Close()
			st.conn = nil
		}
		st.mu.Unlock()
	}
}

func recvExact(c io.Reader, n int) ([]byte, error) {
	buf := make([]byte, n)
	if _, err := io.ReadFull(c, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

func hello(sock, nonce string) (io.ReadWriteCloser, string, error) {
	var c io.ReadWriteCloser
	var err error
	if strings.HasPrefix(sock, `\\.\pipe\`) {
		c, err = dialPipe(sock)
	} else {
		c, err = dial(sock)
	}
	if err != nil {
		return nil, "", err
	}
	hb, _ := json.Marshal(map[string]any{
		"session_token": os.Getenv("RETRACE_SESSION"),
		"nonce":         nonce,
		"pid":           os.Getpid(),
		"ppid":          os.Getppid(),
		"boot_id":       "goruntime",
		"cmdline":       strings.Join(os.Args[:min(4, len(os.Args))], " "),
		"retrace_version": "goretrace-1",
	})
	if _, err := c.Write(frame(mHello, hb)); err != nil {
		c.Close()
		return nil, "", err
	}
	// WELCOME (a POLICY_SET may trail; runtime agents are
	// observers -- anything after the welcome is drained by
	// the beat loop's ignore-reader)
	if dl, ok := c.(interface{ SetReadDeadline(time.Time) error }); ok {
		dl.SetReadDeadline(time.Now().Add(5 * time.Second))
		defer dl.SetReadDeadline(time.Time{})
	}
	hdr, err := recvExact(c, 12)
	if err != nil {
		c.Close()
		return nil, "", err
	}
	if string(hdr[:4]) != magic {
		c.Close()
		return nil, "", io.ErrUnexpectedEOF
	}
	ln := binary.LittleEndian.Uint32(hdr[8:])
	body, err := recvExact(c, int(ln))
	if err != nil {
		c.Close()
		return nil, "", err
	}
	if binary.LittleEndian.Uint16(hdr[6:]) != mWelc {
		c.Close()
		return nil, "", io.ErrUnexpectedEOF
	}
	var w struct {
		AgentID string `json:"agent_id"`
	}
	json.Unmarshal(body, &w)
	return c, w.AgentID, nil
}

func agentLoop() {
	defer close(st.done)
	backoff := 500 * time.Millisecond
	for {
		st.mu.Lock()
		stopped := st.stop
		st.mu.Unlock()
		if stopped == nil {
			return
		}
		select {
		case <-stopped:
			return
		default:
		}
		c, id, err := hello(st.sock, st.nonce)
		if err != nil {
			select {
			case <-stopped:
				return
			case <-time.After(backoff):
			}
			if backoff < 10*time.Second {
				backoff *= 2
			}
			continue
		}
		backoff = 500 * time.Millisecond
		st.mu.Lock()
		st.conn = c
		st.agentID = id
		st.mu.Unlock()
		go drain(c) // daemon pushes (policy/ping) are read+dropped
		for {
			st.mu.Lock()
			dead := st.conn == nil
			st.mu.Unlock()
			if dead {
				break // drain saw EOF; reconnect
			}
			select {
			case <-stopped:
				send(mBye, map[string]any{"agent_id": st.agentID})
				st.mu.Lock()
				if st.conn != nil {
					st.conn.Close()
					st.conn = nil
				}
				st.mu.Unlock()
				return
			case <-time.After(beatDur):
				st.mu.Lock()
				seq := st.seq
				st.mu.Unlock()
				send(mBeat, map[string]any{
					"agent_id": st.agentID,
					"seq":      seq,
				})
			}
		}
	}
}

func drain(c io.ReadWriteCloser) {
	buf := make([]byte, 4096)
	for {
		if _, err := c.Read(buf); err != nil {
			st.mu.Lock()
			if st.conn == c {
				st.conn = nil
			}
			st.mu.Unlock()
			return
		}
	}
}

// Emit sends one runtime-attributed event (public API).
func Emit(name string, attrs map[string]string) {
	st.mu.Lock()
	st.seq++
	seq := st.seq
	agent := st.agentID
	st.mu.Unlock()
	payload := map[string]any{
		"agent_id": agent,
		"seq":      seq,
		"ts":       time.Now().Unix(),
		"name":     name,
		"attrs":    attrs,
		"source":   "runtime",
	}
	send(mEvent, payload)
}

// hookRT wraps an http.RoundTripper with per-request events.
type hookRT struct {
	next http.RoundTripper
}

func (h hookRT) RoundTrip(req *http.Request) (*http.Response, error) {
	Emit("go.http.request", map[string]string{
		"method": req.Method,
		"host":   req.URL.Host,
		"path":   req.URL.Path,
	})
	resp, err := h.next.RoundTrip(req)
	if err != nil {
		Emit("go.http.error", map[string]string{
			"host": req.URL.Host, "error": err.Error()})
		return resp, err
	}
	Emit("go.http.response", map[string]string{
		"host":   req.URL.Host,
		"path":   req.URL.Path,
		"status": itoa(resp.StatusCode),
	})
	return resp, nil
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	neg := n < 0
	if neg {
		n = -n
	}
	var b [8]byte
	i := len(b)
	for n > 0 {
		i--
		b[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		b[i] = '-'
	}
	return string(b[i:])
}

// HookHTTP wraps http.DefaultTransport with the observing
// RoundTripper (idempotent).
func HookHTTP() {
	st.mu.Lock()
	defer st.mu.Unlock()
	if st.hooked {
		return
	}
	st.hooked = true
	t, ok := http.DefaultTransport.(http.RoundTripper)
	if !ok || t == nil {
		t = http.DefaultTransport
	}
	http.DefaultTransport = hookRT{next: t}
}
