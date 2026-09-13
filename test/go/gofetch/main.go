/*
 * The Go runtime lane's E2E workload (TODO.impl/12): a cgo
 * build (CGO_ENABLED=1) that SUPERVISES (runtime lane) and
 * fetches by NAME (the cgo resolver's getaddrinfo is the libc
 * lane when the retrace preload rides along -- one process,
 * two evidence lanes, graded side by side).
 *
 * usage: gofetch <url>
 */
package main

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"time"

	retrace "retrace/retrace"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: gofetch <url>")
		os.Exit(2)
	}
	if !retrace.Supervise() {
		fmt.Fprintln(os.Stderr, "supervise: env absent")
		os.Exit(3)
	}
	retrace.HookHTTP()
	retrace.Emit("go.test.marker", map[string]string{
		"kind": "direct"})

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get(os.Args[1])
	if err != nil {
		fmt.Fprintln(os.Stderr, "get:", err)
		os.Exit(4)
	}
	n, _ := io.Copy(io.Discard, resp.Body)
	resp.Body.Close()
	fmt.Printf("fetched %d bytes status %d\n", n, resp.StatusCode)

	time.Sleep(1500 * time.Millisecond) // beats settle
	retrace.Shutdown()
}
