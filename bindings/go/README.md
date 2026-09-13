# retrace Go binding — the Go runtime agent

The Go lane of the supervisor fleet (TODO.impl/12): the same
RTRD protocol the Python/Node/JVM agents speak, with a cgo-free
hook surface — the runtime's own boundaries instead of libc
symbols.

## Quick start

```go
import retrace "retrace/retrace"

func main() {
	retrace.Supervise()   // joins RETRACE_SUPERVISOR env; no-op absent
	retrace.HookHTTP()    // wraps http.DefaultTransport
	defer retrace.Shutdown()

	retrace.Emit("go.marker", map[string]string{"kind": "custom"})
}
```

With the supervisor env armed (`retraced --sock ... --nonce ...`),
each request lands in the daemon journal as a runtime-attributed
event — `go.http.request {method,host,path}` and
`go.http.response {status}` — beside the libc lane when the
retrace preload rides the same process.

## The two lanes (E2E shape)

The E2E target (`test/go/gofetch`) is a `netcgo` build: its name
resolution wants libc. One process, two evidence lanes: the
runtime agent's HTTP events in the journal; the preload's
interposed `write` calls (scoped config) in the retrace log.

Known gap (recorded on the card): the FULL function inventory
under a cgo Go binary trips value-result hazards — `getsockopt`
returns EFAULT and interposed `getaddrinfo` breaks resolution.
Interposition-heavy Go tracing needs those out-param prototypes
fixed in retrace core; the lane demonstration scopes around
them.

## Layout

- `retrace/retrace.go` — the agent (framing, UDS + named-pipe
  transports, heartbeat/BYE, `Emit`, `HookHTTP`)
