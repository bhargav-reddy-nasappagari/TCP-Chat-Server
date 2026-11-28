# TCP Chat Server in C: Design Specification

Originally written Oct 18, 2026 by @Bunny. Converted to Markdown for `docs/DESIGN.md` and synced with the code after M0 (see the change log at the end).

## 1. Purpose, scope and success criteria

This document specifies a multi-client TCP chat server in C for Linux, small enough to build in about 19 hours and complete enough to defend line by line in an interview. The MVP is a win if it does everything claimed in the four project bullets on your resume, and each claim can be demonstrated live.

**What the system is:** one server program (`chatd`, built as `tcpchat`) that accepts many simultaneous TCP connections and relays each client's text lines to every other client. Any line-oriented TCP client, such as `nc`, can use it. A small client (`chat`) is an optional extra.

**In scope:** concurrent connections, broadcast, nicknames, three commands (`/nick`, `/who`, `/quit`), a line-framing protocol that survives partial reads and writes, clean handling of disconnects and errors, graceful shutdown, a Makefile, and automated tests.

**Out of scope (deliberately):** encryption, authentication, chat rooms, private messages, message history, persistence, IPv6, and event-driven (`epoll`) scaling. Each is a documented upgrade path in section 11, not a hidden gap.

### The four resume claims this MVP must prove

| Ref | Resume claim | Demonstrated by |
|---|---|---|
| R1 | Multi-client chat server in C using TCP sockets on Linux, accepting concurrent connections and broadcasting messages | 3 or more clients exchange messages live; a test with 50 clients passes |
| R2 | POSIX threads and a mutex-protected shared client state to avoid race conditions; clean connect and disconnect handling | ThreadSanitizer reports zero races under load; connect/disconnect churn leaves no leaked file descriptors |
| R3 | A simple message framing protocol handling partial reads and writes, with error handling for failed system calls and dropped connections | A test that sends one byte at a time and a test that sends two lines in one packet both pass; every system call's failure path is handled |
| R4 | Built with a Makefile, debugged using GDB, tested with multiple simultaneous clients | `make` builds and tests run; a documented GDB session found or inspected a real issue; the end-to-end test drives many clients |

The skills line on the resume (C, Linux, GCC, Make, socket programming, multithreading, TCP/IP) is covered by the same evidence.

**Definition of done for the whole project:** all Must requirements in section 2 pass their verification, the traceability table in section 9 is complete, and you can explain every module in this document without reading it.

## 2. Requirements

The requirements are deliberately few: 16 Must items cover the whole MVP, and 4 Should items add polish. Functional (F) and non-functional (NF) items sit in one table because the non-functional ones decide how the functional ones are built: for example, NF-1 (no data races) is why FR-3 (broadcast) takes a lock.

| ID | Type | Requirement | Priority | Verified by | Resume |
|---|---|---|---|---|---|
| FR-1 | F | Listen on a TCP port (default 5000, overridable by argument) and accept connections concurrently, up to 64 clients. A 65th client is told `! server full` and disconnected. | Must | E2E: connect 64, then a 65th | R1 |
| FR-2 | F | Every client gets a unique default nickname (`user<N>`) on connect, a welcome line, and a join notice to the others. | Must | E2E: join notices seen | R1 |
| FR-3 | F | A chat line from one client is delivered, in order and unbroken, to every other connected client, formatted `nick: text`. The sender does not receive its own line. | Must | E2E: 3 clients, order and content checked | R1 |
| FR-4 | F | `/nick <name>` changes the nickname (1 to 16 characters from `A-Za-z0-9_-`, unique ignoring case) and announces it. `/who` lists connected nicknames. `/quit` disconnects. Unknown commands get an error line. | Must | Unit: command parsing. E2E: each command | R1 |
| FR-5 | F | Messages are newline-terminated lines of at most 512 bytes including the newline. `\r\n` is accepted. Empty lines are ignored. Lines containing control characters are rejected with an error line. | Must | Unit: linebuf and proto tests | R3 |
| FR-6 | F | A line split across any number of TCP segments is reassembled; several lines in one segment are all delivered. | Must | E2E: send one byte at a time; send 3 lines in one write | R3 |
| FR-7 | F | Every outbound write handles partial writes by looping until all bytes are sent or the connection fails. | Must | Unit test of `net_send_all` on a socketpair with a small buffer | R3 |
| FR-8 | F | A client that disconnects in any way (orderly close, reset, send timeout, over-long line, `/quit`) is removed exactly once, its descriptor closed exactly once, and a leave notice sent to the others. | Must | E2E: each disconnect style; descriptor count back to baseline | R2 |
| FR-9 | F | On SIGINT or SIGTERM the server stops accepting, tells clients `* server shutting down`, closes every connection, waits up to 5 seconds for sessions to finish, and exits with status 0. | Must | E2E: send SIGINT, check exit code and time | R2 |
| FR-10 | F | A client program `chat` reads the keyboard and the socket at the same time using `poll`, prints incoming lines, and sends typed lines. | Should | Manual plus E2E smoke test | R1 |
| NF-1 | NF | Concurrency safety: no data races and no deadlocks under 50 clients sending continuously. | Must | ThreadSanitizer run of the E2E stress test | R2 |
| NF-2 | NF | Memory and descriptor safety: no leaks, no invalid accesses; after 200 connect/disconnect cycles the server's open descriptor count equals its starting count. | Must | Valgrind, AddressSanitizer, `/proc/<pid>/fd` count | R2 |
| NF-3 | NF | Robustness: the server never crashes or hangs on malformed input, binary bytes, oversized lines, abrupt resets, SIGPIPE, or a client that stops reading. One bad client delays others by at most the send timeout (1 second). | Must | E2E fault-injection tests | R3 |
| NF-4 | NF | Every system call's return value is checked; EINTR is retried where appropriate; failures are logged with errno text. | Must | Code review checklist; `-Wall -Wextra -Werror` plus `warn_unused_result` on wrappers | R3 |
| NF-5 | NF | Build: C11 and POSIX only, no external libraries, zero warnings with `-Wall -Wextra -Werror`, one `make` command builds everything and `make test` runs all tests. | Must | CI-style local run | R4 |
| NF-6 | NF | Modularity: each module has one purpose, a header-only public interface, no circular dependencies, and no shared global state except one shutdown flag. | Must | Dependency check in section 3; review | R4 |
| NF-7 | NF | Testability: the framing and command-parsing logic is pure (no sockets, no threads) so it is unit-testable in isolation. | Must | Unit tests run without a network | R4 |
| NF-8 | NF | Observability: one log line per connect, disconnect, error and shutdown event, with timestamp, level and client id. | Should | Manual inspection | R2 |
| NF-9 | NF | Debuggability: a debug build with `-g -O0`; one documented GDB session (attach, `thread apply all bt`, inspect a client slot). | Should | README section | R4 |
| NF-10 | NF | Footprint: each session thread uses a 256 KiB stack; no heap allocation on the message path. | Should | Code review | R2 |

**Fixed limits** (all compile-time constants in `config.h`): 64 clients, 512 bytes per line, 16-character nicknames, 1 second send timeout, listen backlog 16, 5 second shutdown wait.

## 3. Architecture

The system is eight modules in five layers. The rule that keeps it loosely coupled: **a module may call only modules in a layer below it.** No upward calls, no calls between modules of the same layer, no cycles.

```
Layer 4   server / main        accept loop, thread creation, shutdown
             |
Layer 3   session              the life of one client
             |
Layer 2   registry             shared client table, the only lock
             |
Layer 1   net    linebuf    proto
             |
Layer 0   log    config.h
```

Read the diagram from the top: the server creates sessions, a session uses the registry for shared state and uses `linebuf`, `proto` and `net` for its own bytes, and the registry uses `net` to write. Every module may use `log` and `config.h`.

### Coupling rules, checked in code review

1. A module includes only headers from layers below it, plus `config.h`. `linebuf.h` and `proto.h` include nothing but standard headers and `config.h`, so they cannot touch sockets, threads or the registry.
2. Only `net.c` creates, binds, accepts, sends and receives. Only `registry.c` shuts down or closes the descriptor of a *registered* client. The listening descriptor is closed by `server.c`. A descriptor that never entered the table (rejected because the server is full, or because thread creation failed) is closed by the code that rejected it: `session.c` or `server.c` respectively.
3. Only `registry.c` locks the client table. Only `server.c` touches the session counter.
4. The only global variable in the program is `g_stop`, the shutdown flag.
5. Data crosses a module boundary as plain values, integer ids and buffers owned by the caller. No module returns a pointer into another module's internal state; registry hands out `client_id_t` values and copies nicknames into caller buffers, never pointers into its slots.

**Why these particular modules (cohesion):** each has exactly one reason to change, so a future change touches one file.

**Why the lines fall where they do (coupling):** the two pure modules (`linebuf`, `proto`) know nothing about networking, so they are tested with plain strings. The registry is the only meeting point between threads, so the entire concurrency argument can be checked in one file. The session knows how to run a client but not how threads are created, and the server knows how to run a process but not what a chat message is.

## 4. Module specifications

There are eight modules plus the build and test files. Each one has a single purpose (high cohesion) and talks to its neighbours only through the functions in its header (loose coupling).

| Module | Its single reason to change |
|---|---|
| net | How sockets are used (for example IPv6 or TLS) |
| linebuf | How messages are framed |
| proto | What the chat language looks like |
| registry | How shared state is protected |
| session | What a client may do |
| server | How the process starts, runs and stops |

| Module | Layer | One-line purpose | Knows about sockets? | Knows about threads? | Knows about chat? |
|---|---|---|---|---|---|
| config.h | 0 | Every tunable constant in one place | no | no | no |
| log | 0 | Thread-safe timestamped logging | no | no | no |
| net | 1 | Safe wrappers over socket system calls | yes | no | no |
| linebuf | 1 | Turn a byte stream into lines | no | no | no |
| proto | 1 | Parse input lines and format output lines | no | no | yes (syntax only) |
| registry | 2 | The shared table of connected clients, and the only owner of the lock | uses net to send | holds the mutex | yes (state only) |
| session | 3 | The life of one client, from first byte to removal | via net | runs in a thread | yes |
| server / main | 4 | Accept loop, thread creation, shutdown coordination | listen and accept | creates threads | no |

### 4.1 `config.h`: constants and attributes

**Purpose:** one place for limits, so every module agrees and tests can shrink them.

```c
#define MAX_CLIENTS       64
#define LINE_MAX_BYTES    512                 /* includes the trailing \n */
#define NICK_MAX          16
#define OUT_MAX           (LINE_MAX_BYTES + NICK_MAX + 8)
#define SEND_TIMEOUT_MS   1000
#define LISTEN_BACKLOG    16
#define SHUTDOWN_WAIT_S   5
#define SESSION_STACK     (256 * 1024)
#define DEFAULT_PORT      5000
#define IP_STR_MAX        46                  /* INET6_ADDRSTRLEN, with NUL */

#define WARN_UNUSED       __attribute__((warn_unused_result))
#define PRINTF_FMT(f, a)  __attribute__((format(printf, f, a)))
```

**Why:** magic numbers scattered across files are the usual source of mismatched buffer sizes, which cause overflows. `IP_STR_MAX` replaces a bare `46` in `session.h`. The two attribute macros are not constants, but several headers need them and a separate header would add a ninth module, so they live here. `WARN_UNUSED` is how NF-4 is enforced by the compiler; `PRINTF_FMT` turns format-string mistakes into compile errors under `-Werror`.

### 4.2 `log`: logging

**Purpose:** print one line per event to stderr with a timestamp and level.

```c
enum { LOGL_INFO, LOGL_WARN, LOGL_ERR };
void log_msg(int level, const char *fmt, ...) PRINTF_FMT(2, 3);
void log_errno(const char *what);   /* "what: strerror text of errno", at LOGL_ERR */
```

**Behaviour:** formats the whole line into a local buffer with `localtime_r` and `vsnprintf`, then emits it with a single `fprintf`, so lines from different threads never interleave. `log_errno` uses `strerror_r`, not `strerror`. No global state. Callers include the client id in the format string (for example `log_msg(LOGL_INFO, "client %u connected from %s", id, ip)`), which satisfies NF-8. Logging failures are ignored by design, so these functions return nothing and carry no `WARN_UNUSED`.

**Why:** threads that call `printf` piecemeal produce garbled logs, and debugging a concurrent server without readable logs is slow. `localtime_r` is used instead of `localtime` because the latter uses a static buffer and is not thread-safe. The levels are named `LOGL_*` because `<syslog.h>` defines `LOG_ERR` and `LOG_INFO` as macros, which would clash.

### 4.3 `net`: socket helpers

**Purpose:** hide the unpleasant details of the sockets API, and be the only module that calls `socket`, `bind`, `listen`, `accept`, `send` and `recv`.

```c
int     net_listen(uint16_t port, int backlog);                           /* fd, or -1 */
int     net_accept(int listen_fd, char *ip, size_t ipsz, uint16_t *port); /* fd, 0 = timeout, -1 = error */
int     net_set_send_timeout(int fd, int ms);                             /* 0 or -1 */
ssize_t net_send_all(int fd, const char *buf, size_t len);                /* len, or -1 */
ssize_t net_recv(int fd, char *buf, size_t cap);                          /* >0 bytes, 0 = peer closed, -1 = error */
```

All five are declared `WARN_UNUSED`.

**Behaviour:**

- `net_listen`: `socket`, `SO_REUSEADDR`, `bind`, `listen`; closes the descriptor on any failure so nothing leaks.
- `net_accept`: waits with `poll` for up to 500 ms so the caller can notice the shutdown flag; retries on `EINTR`; fills the peer address with `inet_ntop` (the buffer should be `IP_STR_MAX` bytes); enables `SO_KEEPALIVE`.
- `net_send_all`: loops until all bytes are written; each pass uses `send` with `MSG_NOSIGNAL`; retries on `EINTR`; returns -1 on any other error including a send timeout (`EAGAIN`).
- `net_recv`: one `recv`, retried on `EINTR`.

**Why:** TCP is a byte stream, so one `send` may write fewer bytes than requested and one `recv` may return any number of bytes. Putting the loops and errno handling in one tested place means no other module can forget them. `MSG_NOSIGNAL` stops a write to a dead peer from killing the process with SIGPIPE.

### 4.4 `linebuf`: framing

**Purpose:** convert arbitrary chunks of bytes into complete lines. Pure logic: no I/O, no allocation, no locks.

```c
typedef struct { char data[LINE_MAX_BYTES]; size_t len; } linebuf_t;
typedef enum { LB_LINE, LB_NEED_MORE, LB_TOO_LONG } lb_status_t;

void        lb_init(linebuf_t *lb);
char       *lb_space(linebuf_t *lb, size_t *cap);   /* where recv should write; cap = free bytes */
void        lb_commit(linebuf_t *lb, size_t n);     /* n bytes were just written there */
lb_status_t lb_next(linebuf_t *lb, char *out, size_t outsz);  /* WARN_UNUSED; one line, without \n or \r */
```

**Behaviour:** session calls `recv` directly into the free space, commits the byte count, then calls `lb_next` repeatedly until it returns `LB_NEED_MORE`. A line is complete when a `\n` is found. If the buffer is full and holds no `\n`, the result is `LB_TOO_LONG`. After a line is extracted the remaining bytes shift to the front, so a partial next line is kept. `out` is NUL-terminated and `outsz` should be at least `LINE_MAX_BYTES`.

**Why:** this is the heart of requirement R3. Isolating it makes the hardest TCP concept testable with a few lines of C and no network: feed it one byte at a time, feed it three lines at once, feed it 600 bytes with no newline. Receiving straight into the buffer avoids an extra copy and any temporary array.

### 4.5 `proto`: the chat language

**Purpose:** define what a line means and how replies are written. Pure logic.

```c
typedef enum { P_EMPTY, P_CHAT, P_NICK, P_WHO, P_QUIT, P_UNKNOWN, P_INVALID } ptype_t;
typedef struct { ptype_t type; const char *arg; } parsed_t;   /* arg points into the input line */

parsed_t proto_parse(char *line);                 /* may edit the line in place (trim, split) */
int      proto_valid_nick(const char *s);         /* 1 valid, 0 not */
int      proto_fmt_chat(char *out, size_t sz, const char *nick, const char *text);
int      proto_fmt_notice(char *out, size_t sz, const char *fmt, ...);   /* "* ...\n" */
int      proto_fmt_error(char *out, size_t sz, const char *fmt, ...);    /* "! ...\n" */
```

`proto_valid_nick` and the three formatters are `WARN_UNUSED`; the two variadic formatters also carry `PRINTF_FMT(3, 4)`.

**Behaviour:** trims a trailing `\r`, classifies the line, rejects control characters (`P_INVALID`), and validates nicknames. Formatting functions always end lines with `\n` and never write past `sz`. They return the length written (excluding the NUL), or -1 if the output did not fit, in which case the buffer must not be sent.

**Why:** keeping the chat language apart from both the byte-stream logic and the networking means you can change a command without touching the sockets, and unit-test every command with plain strings.

### 4.6 `registry`: shared client state

**Purpose:** own the table of connected clients and the single mutex that protects it. This is the only module that holds a lock, and the only module that closes a client's socket.

```c
typedef uint32_t client_id_t;   /* never reused during one run; 0 means "none" */

int    reg_add(int fd, const char *ip, client_id_t *id, char *nick_out);  /* 0 ok, -1 full */
int    reg_remove(client_id_t id, char *nick_out);          /* 1 if removed now, 0 if already gone; closes the fd */
int    reg_set_nick(client_id_t id, const char *nick, char *old_out);    /* 0 ok, -1 taken or unknown id */
int    reg_get_nick(client_id_t id, char *out, size_t outsz);            /* 0 ok, -1 unknown id */
int    reg_send_to(client_id_t id, const char *msg, size_t len);         /* 0 ok, -1 unknown, dying or write failed */
int    reg_broadcast(client_id_t from, const char *msg, size_t len);     /* count delivered; from = 0 means everyone */
int    reg_who(char *out, size_t sz);                       /* comma-separated nicknames; returns how many */
void   reg_kick_all(void);                                  /* shutdown(fd) on every client, to wake blocked threads */
size_t reg_count(void);
```

Every function that returns a value is `WARN_UNUSED`. Buffers that receive a nickname (`nick_out`, `old_out`) must hold at least `NICK_MAX + 1` bytes. There is no `reg_init`: the mutex is a static `PTHREAD_MUTEX_INITIALIZER` and the table is zero-initialised.

**State:** a fixed array of 64 slots, each holding `in_use`, `id`, `fd`, `nick`, `ip` and a `dying` flag, plus one `pthread_mutex_t`.

**Rules the module enforces:**

1. Every function takes the lock on entry and releases it on every return path.
2. `recv` is never called while holding the lock.
3. Writes to a client socket happen only inside registry functions, under the lock, so two threads can never interleave bytes on one connection. This also guarantees every broadcast message reaches all recipients as one unbroken unit and in one global order.
4. If a write to a client fails or times out, the registry calls `shutdown(fd, SHUT_RDWR)` and sets `dying`. It does not close the descriptor: the owning session thread is blocked in `recv`, wakes up with 0 or an error, and calls `reg_remove`, which is the single place where the descriptor is closed and the slot freed. One cleanup path means no double close.
5. Ids come from a counter that never wraps during a run (starting at 1), so a stale id can never match a new client that took over the same slot.

**Why:** shared mutable state is where concurrent programs fail. Putting all of it, the lock and the rules in one module means the whole concurrency argument can be read in about 200 lines. Nothing outside can touch the table, so nothing outside can forget to lock it. `reg_get_nick` exists because the session needs its own nickname to format `nick: text`; only the owning session thread changes a client's nickname, so the get-then-broadcast sequence cannot be stale.

### 4.7 `session`: one client's life

**Purpose:** run the per-client loop. One session runs in one thread.

```c
typedef struct { int fd; char ip[IP_STR_MAX]; } session_arg_t;   /* allocated by server, freed by session */
void session_run(session_arg_t *arg);
```

**Behaviour** (summary; full sequence in section 6): register the client, welcome it, announce the join, then loop: `recv` into the `linebuf`, extract lines, parse each with `proto`, and act on it through `registry`. On any exit, call `reg_remove` once and announce the leave. Where the session deliberately ignores a `WARN_UNUSED` result (for example the result of `reg_broadcast`, since the registry already handles failed recipients), it says so with an explicit `(void)` cast.

**Why:** keeping the per-client logic in one function that uses only `net`, `linebuf`, `proto` and `registry` means it can be read top to bottom as a story, and it never needs to know how threads are created or how the process is shut down.

### 4.8 `server` and `main`: the process

**Purpose:** `main.c` parses the port argument, installs signal handlers, and calls `server_run`. `server.c` owns the listening socket, the accept loop, thread creation, and shutdown.

```c
int server_run(uint16_t port);              /* returns the process exit code */
extern volatile sig_atomic_t g_stop;        /* the only global; defined in server.c, set by the signal handler */
```

**Behaviour:**

- **Signals:** SIGINT and SIGTERM set `g_stop`; SIGPIPE is ignored. Handlers are installed with `sigaction` and no `SA_RESTART`.
- **Accept loop:** `net_accept` returns every 500 ms even if idle; the loop checks `g_stop` each time. For each connection: allocate a `session_arg_t`, set the send timeout, create a detached thread with a 256 KiB stack that runs a small wrapper (`session_run`, then decrement the active-session counter). If any step fails, close the socket and free the argument.
- **Active-session counter:** an `int` guarded by its own mutex and condition variable inside `server.c`.
- **Shutdown:** close the listening socket, broadcast `* server shutting down`, call `reg_kick_all`, then wait on the condition variable until the counter reaches zero or 5 seconds pass; log how many sessions were still running, and exit.

**Why:** process-level concerns (signals, thread creation, exit) are separate from client-level concerns, so session can be tested or reused without them. The counter exists because detached threads cannot be joined, and the server must still know when it is safe to exit.

### 4.9 Optional client `chat.c`

**Purpose (Should):** a small interactive client. It connects with `net`, then loops on `poll` over stdin and the socket, using `linebuf` to reassemble incoming lines and `net_send_all` for outgoing ones. It is single-threaded on purpose, to show the `poll` approach beside the thread approach in the server.

### 4.10 Build and test files

- **`Makefile`:** targets `all`, `debug` (`-g -O0`), `asan` (`-fsanitize=address,undefined`), `tsan` (`-fsanitize=thread`), `test`, `clean`. Flags: `-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pthread`, plus `-MMD -MP` for header dependency tracking. Each variant builds into its own directory (`build/release/`, `build/debug/`, `build/asan/`, `build/tsan/`), and the Makefile creates them itself.
- **`tests/test_linebuf.c`, `tests/test_proto.c`, `tests/test_net.c`:** unit tests using `assert`; no framework.
- **`tests/e2e.py`:** end-to-end tests using Python's standard `socket` module to open many clients, inject faults, and check results. Python is used only for test tooling, because writing 50 concurrent clients is far quicker there.

## 5. Protocol specification

The protocol is plain text over TCP, one message per line. It is simple enough to use with `nc localhost 5000` and strict enough to be fully specified.

**Framing rule.** TCP delivers a stream of bytes with no message boundaries: one `send` can arrive as several `recv` calls, and several `send` calls can arrive as one. A message therefore ends at a newline byte (`\n`), not at the end of a packet. Everything the server reads is accumulated in a per-client buffer until a newline appears.

| Rule | Value | Reason |
|---|---|---|
| Line terminator | `\n` (a preceding `\r` is stripped) | Works with `nc`, `telnet` and scripts |
| Maximum line | 512 bytes including the terminator | Bounds memory per client; one fixed buffer |
| Allowed bytes | Printable ASCII and UTF-8 text; control characters below 0x20 are rejected | Prevents NUL bytes truncating C strings and terminal escape tricks |
| Empty line | Ignored | Harmless, avoids noise |
| Over-long line | Server sends `! line too long` and disconnects | A client that breaks framing cannot be trusted to resynchronise |

### Client to server

A line that starts with `/` is always a command. To send chat text that starts with `/`, there is no escape in the MVP; this is a documented limitation.

| Input | Meaning | Server reaction |
|---|---|---|
| `hello everyone` | Chat text | Broadcast `nick: hello everyone` to all other clients |
| `/nick alice` | Change nickname | If valid and free: broadcast `* user7 is now known as alice`; else reply `! nickname invalid` or `! nickname taken` to that client only |
| `/who` | List users | Reply to that client only: `* online (3): alice, bob, user9` |
| `/quit` | Leave | Reply `* goodbye`, then disconnect and announce the leave |
| `/anything-else` | Unknown command | Reply `! unknown command` to that client only |
| Line with control characters | Invalid | Reply `! invalid characters`; the connection stays open |

### Server to client

Every server line begins with a one-character class marker so any client can tell them apart without parsing:

| Form | Class | Examples |
|---|---|---|
| `nick: text` | Chat from another user | `alice: hi all` |
| `* text` | Information (join, leave, rename, who, welcome, shutdown) | `* user7 joined`, `* bob left`, `* server shutting down` |
| `! text` | Error addressed to this client | `! server full`, `! nickname taken` |

A nickname cannot contain `:`, a space, `*` or `!`, so a chat line can never be mistaken for an information or error line.

### Nicknames

1 to 16 characters from `A-Za-z0-9_-`. Uniqueness is checked case-insensitively, so `Bob` and `bob` cannot coexist (this prevents impersonation by case). New clients start as `user<N>` where N is the client id.

### Example session

```
(client connects)
* Welcome user7. 2 online. Commands: /nick /who /quit
/nick alice
* user7 is now known as alice
hello everyone
(other clients see:) alice: hello everyone
/quit
* goodbye
```

### Why this design

- **Newline framing instead of a length prefix:** a length prefix is slightly more efficient but cannot be typed by hand, so `nc` could not test the server. The reassembly problem is identical in both, so the learning value for requirement R3 is the same, and debugging is far easier.
- **Class markers instead of structured fields:** one character is enough to separate chat, information and errors, and it keeps the parser and the client trivial.
- **Disconnect on over-long lines but not on invalid characters:** an over-long line means the framing is lost (we do not know where the next line starts), whereas an invalid-character line is a complete line that is merely refused.

## 6. System behaviour

This section describes what the running system does, why it behaves that way, and how the code achieves it.

### 6.1 Threads at run time

The process has one main thread plus one thread per connected client. The main thread only accepts connections and creates threads. Each session thread spends nearly all its time blocked in `recv` waiting for its own client, which costs nothing while idle. With 64 clients that is 65 threads, each with a 256 KiB stack, about 16 MiB of virtual memory in total.

**Why one thread per client:** a blocking `recv` per client makes each session's code a straight line (read, parse, act, repeat) with no state machine or event loop to reason about. It is the easiest design to explain and debug, and 64 clients is far below the point where threads become a cost. **How:** `server.c` creates each thread with `pthread_create` in the detached state and a small stack set through `pthread_attr_setstacksize`.

### 6.2 Client lifecycle

Every client moves through the states below. A state can only be left through the arrows shown, which is what makes cleanup exactly-once.

```
accepted --(server full / thread creation fails)--> REJECTED   (socket closed, no table entry)
   |
   v  reg_add ok
ACTIVE --(write fails or times out)--> DYING --+
   |                                           |
   +--(recv 0 / error, /quit, over-long line)--+--> REMOVED   (reg_remove: fd closed, slot freed)
```

A client is Active until something ends its read loop. A failed write moves it to Dying first, so later broadcasts skip it, and then it follows the same road as every other exit into Removed. The Rejected state covers a full server and a failed thread creation.

### 6.3 Connection sequence (accept to first message)

1. **Accept.** `net_accept` returns a new descriptor and the peer address. The server allocates a `session_arg_t`, sets the 1 second send timeout, and creates a detached thread. If thread creation fails, the server closes the descriptor, frees the argument and logs the error; the client sees an immediate close, and the server keeps running.
2. **Register.** The session thread calls `reg_add`. If all 64 slots are used it returns -1; the session sends `! server full` directly (the client is not in the table, so no other thread can write to it), closes the descriptor, and exits. Otherwise the client gets an id and the default nickname `user<id>`.
3. **Welcome.** The session sends `* Welcome user7. 2 online. Commands: /nick /who /quit` to the new client only, then broadcasts `* user7 joined` to everyone else.
4. **Serve.** The read loop starts.

**Why register inside the session thread rather than the accept loop:** the accept loop must never block, and registration, welcoming and broadcasting all involve sends that could wait up to the send timeout. Doing them in the client's own thread keeps the accept loop responsive to new connections and to shutdown.

### 6.4 The read loop and message flow

```
loop:
    space = lb_space(&lb, &cap)
    n = net_recv(fd, space, cap)
    n > 0 : lb_commit(&lb, n)
            while (lb_next(&lb, line, sizeof line) == LB_LINE) handle(line)
            if lb_next returned LB_TOO_LONG : send error, leave loop
    n == 0 : peer closed in an orderly way, leave loop
    n < 0 : log errno, leave loop (reset, timeout, shutdown by registry or by server)

handle(line): p = proto_parse(line)
    P_CHAT    : reg_get_nick, format "nick: text", reg_broadcast(my_id, ...)
    P_NICK    : reg_set_nick, then broadcast the rename notice or send an error to me
    P_WHO     : reg_who, then reg_send_to(my_id, ...)
    P_QUIT    : send "* goodbye", leave loop
    P_UNKNOWN : reg_send_to(my_id, "! unknown command")
    P_INVALID : reg_send_to(my_id, "! invalid characters")
    P_EMPTY   : nothing

after loop: if reg_remove(my_id) removed me now, broadcast "* nick left"
```

**How a chat line travels from alice to bob:** alice's thread reads the bytes, `linebuf` finds the newline, `proto` classifies the line as chat, and `registry` takes the lock, walks all 64 slots, writes the formatted line to every in-use slot except alice's, and releases the lock. Bob's thread is blocked in `recv` and is not involved: the broadcast writes directly to bob's socket, and bob's kernel delivers it to bob's client program.

**Why the sender writes to the other clients' sockets directly:** it avoids a per-client outgoing queue and a second thread per client, which would double the thread count and add queue-locking logic. The price is that a slow reader can stall a broadcast, addressed in 6.6.

**`/who` reply size:** a full table of 16-character nicknames would not fit in one 512-byte line, so the `* online (N): ...` reply is built into a buffer of `OUT_MAX` bytes and the nickname list is truncated to fit. The count `N` is always exact. See open decision OD1 in section 11.

### 6.5 Locking model

The two mutexes are never held at the same time, so there is no lock ordering to get wrong, and a deadlock between them is impossible. Within the registry there is only one lock, so there is no ordering problem at all.

| Lock | Protects | Held while | Never held while |
|---|---|---|---|
| registry mutex (one) | The slot table and all writes to client sockets | Scanning the table, writing to sockets (bounded by the send timeout) | Calling `recv`, parsing, logging at length, creating threads |
| server counter mutex (one) | The active-session count and its condition variable | Incrementing or decrementing the counter, waiting on the condition | Taking the registry mutex |

**Why one coarse lock instead of one per client:** a per-client lock invites lock-ordering bugs (two threads each holding one client's lock and wanting another's). With at most 64 clients and tiny critical sections, one lock costs almost nothing and is provably deadlock-free. It also gives a useful property: because a whole broadcast happens inside one critical section, every client sees broadcasts in the same order, and no message is ever interleaved with another.

**Races this prevents:**

- Two clients registering at once and taking the same slot.
- A broadcast reading a slot while another thread is removing that client and closing its descriptor, which could send the message to a descriptor number that was immediately reused by a new client.
- Two `/nick` requests claiming the same name at the same moment (the uniqueness check and the assignment happen in one critical section).
- Two writers interleaving bytes on one connection.

### 6.6 Slow or dead clients

Writes happen under the registry lock, so a client that stops reading could stall everyone. The defences, in order:

1. Every client socket has `SO_SNDTIMEO` of 1 second: a blocked write fails with `EAGAIN` instead of waiting forever.
2. On failure the registry marks the slot dying and calls `shutdown(fd, SHUT_RDWR)`; later broadcasts skip dying slots immediately, so each bad client costs at most one timeout.
3. The shutdown wakes the client's own thread out of `recv`, which then runs the normal removal path.
4. `SO_KEEPALIVE` lets the kernel eventually detect peers that vanished without closing (for example a pulled network cable), so those threads do not sit in `recv` forever.

The worst case is a broadcast delayed by one second per simultaneously stuck client, which is stated as NF-3 and accepted for the MVP.

### 6.7 Disconnect handling

All of these end the read loop and take the same cleanup path:

- **Orderly close:** `recv` returns 0.
- **Reset or network error:** `recv` returns -1 with `ECONNRESET`, `ETIMEDOUT` or similar.
- **Registry shutdown after a failed write:** `recv` returns 0 or -1.
- **`/quit` and over-long line:** the loop exits on purpose.
- **Server shutdown:** `reg_kick_all` produces the same wake-up as the registry shutdown above.

**Cleanup path (identical every time):** call `reg_remove`. Under the lock it checks the id, closes the descriptor, clears the slot, and returns whether this call did the removal. Only if it did, the session broadcasts `* nick left`. A second removal attempt for the same id finds nothing and does nothing. This single path is what delivers requirement FR-8 (exactly once) and prevents double-close, descriptor leaks and duplicate leave notices.

### 6.8 Graceful shutdown

1. SIGINT or SIGTERM runs the handler, which only sets `g_stop`. A signal handler may safely do almost nothing, so all real work happens outside it.
2. The accept loop's `poll` times out within 500 ms, sees `g_stop`, and leaves the loop.
3. The server closes the listening socket (no new clients), broadcasts `* server shutting down`, and calls `reg_kick_all`, which shuts down every client socket.
4. Each session thread wakes from `recv`, runs the normal cleanup, decrements the counter, and exits.
5. The server waits on the condition variable until the counter reaches zero, with a 5 second limit. It logs how many sessions were still active if the limit is hit, then returns exit code 0.

**Why wait for the sessions:** exiting while threads still use the registry would be a use-after-free in spirit and would make sanitizer and Valgrind reports noisy. **Why a counter rather than joining threads:** detached threads need no join bookkeeping and cannot leak a thread handle.

## 7. Error handling and robustness

The policy has three rules. **Rule 1:** every system call's result is checked; none is ignored. **Rule 2:** a failure affecting one client ends that client only, never the server. **Rule 3:** a failure that makes the whole server unusable (cannot create the listening socket) is logged and the process exits with a non-zero status.

| Failure | Detected by | Response | Invariant preserved |
|---|---|---|---|
| `socket`, `bind` or `listen` fails at startup (port in use, no permission) | Return value -1 in `net_listen` | Close any open descriptor, log errno text, exit code 1 | No half-open listener, clear message to the operator |
| `accept` interrupted by a signal | `errno == EINTR` | Retry or re-check `g_stop` | Shutdown signal is noticed within 500 ms |
| `accept` fails with `EMFILE` or `ENFILE` (out of descriptors) | errno in `net_accept` | Log, sleep 100 ms to avoid a busy loop, continue | Existing clients unaffected; server recovers when descriptors free up |
| `accept` fails with `ECONNABORTED` | errno | Ignore and continue | A client that gave up before accept never stops the loop |
| `malloc` for `session_arg_t` fails | NULL return | Close the new socket, log, continue | No leak, server keeps running |
| `pthread_create` fails | Non-zero return | Close the socket, free the argument, log | No leak; the one client is dropped, others continue |
| 65th client arrives | `reg_add` returns -1 | Send `! server full`, close, no table change | Table never exceeds 64 entries |
| Client closes normally | `recv` returns 0 | Normal cleanup path | Exactly-once removal (FR-8) |
| Client connection reset | `recv` returns -1, `ECONNRESET` | Log at info level, normal cleanup path | Same cleanup path as every other exit |
| `recv` interrupted by a signal | `EINTR` in `net_recv` | Retry inside the wrapper | Caller never sees `EINTR` |
| Write fails or sends fewer bytes than requested | `send` returns fewer bytes than asked | `net_send_all` loops until done | Messages are never truncated on the wire |
| Write to a peer that has closed | `EPIPE` returned (not a signal, because of `MSG_NOSIGNAL` and SIGPIPE ignored) | Registry marks the slot dying and shuts the socket down | Process is never killed by SIGPIPE |
| Client stops reading; kernel send buffer fills | Send times out with `EAGAIN` after 1 second | Registry marks the slot dying, shuts the socket down | One stuck client delays a broadcast by at most one timeout |
| Client sends a line over 512 bytes with no newline | `lb_next` returns `LB_TOO_LONG` | Send `! line too long`, disconnect | Buffer never overflows; framing is never guessed |
| Client sends control characters or NUL bytes | `proto_parse` returns `P_INVALID` | Reply `! invalid characters`, keep connection | No C-string truncation, no terminal escape injection |
| Client sends a command with bad arguments | `proto_parse` or `proto_valid_nick` | Reply with a specific `!` error | Server state unchanged |
| Two clients claim the same nickname together | Uniqueness check inside the registry lock | One succeeds, the other gets `! nickname taken` | Nicknames are unique at all times |
| Remove called twice for one client (for example the registry kicked it and the session also ends) | `reg_remove` finds no slot for the id | Returns 0; no second leave notice, no second close | No double close, no duplicate notices |
| Client floods the server with data | Bounded buffers and the send timeout | Each line is processed in turn; no unbounded memory | Memory per client is fixed (about 1 KiB of buffer) |
| SIGINT or SIGTERM | Signal handler sets `g_stop` | Graceful shutdown (section 6.8) | All descriptors closed, exit code 0 |
| Sessions do not finish within 5 seconds of shutdown | Timed wait on the condition variable | Log the count that remain, exit anyway | Shutdown never hangs |
| Log write fails | Return value ignored by design | Nothing | Logging can never take down the server |

**Defensive habits applied everywhere:**

- Every buffer has a named size constant, and every copy into it uses a bounded function (`snprintf`, `memcpy` with a checked length).
- Every received line is treated as untrusted: length-checked, then character-checked, before use.
- Descriptors have a single owner at any time, and the code path that closes each one is identified in the design.
- Functions that return error codes are declared `WARN_UNUSED` (`__attribute__((warn_unused_result))`), so the compiler (with `-Werror`) rejects an ignored result. A deliberate ignore is written as an explicit `(void)` cast.

## 8. Design decisions and rationale

Every significant choice, the alternative that was rejected, and the deciding reason. These are also the answers to the interview question "why did you build it this way?".

| # | Decision | Alternative rejected | Why this choice | Cost accepted |
|---|---|---|---|---|
| D1 | One detached POSIX thread per client | `fork` per client; single-threaded `poll`/`epoll` loop; thread pool | Straight-line code per client; matches the resume; threads share the client table naturally | Memory per thread; limited scale (fine at 64); needs locking |
| D2 | One global mutex over the client table | One lock per client; lock-free structures | Provably deadlock-free; simple to review; gives message atomicity and global ordering | All table operations serialise (microseconds, irrelevant at 64 clients) |
| D3 | Fixed array of 64 slots, no dynamic allocation for clients | Linked list or hash table | No malloc on the message path; no per-client leaks; trivial iteration; bounded memory | Hard client limit (a documented, enforced requirement) |
| D4 | Newline-delimited text framing | Length-prefixed binary frames | Testable with `nc`; same reassembly logic to learn; human-readable logs | Text cannot carry raw newlines (not needed for chat) |
| D5 | Receive directly into the line buffer | Receive into a temporary array, then copy | One fewer copy and one fewer buffer to size | Slightly unusual API (`lb_space`, `lb_commit`) that must be explained |
| D6 | Blocking sockets with a 1 second send timeout, writes under the lock | Per-client outgoing queues with writer threads; non-blocking sockets with `epoll` | Smallest correct design; no queue memory management | A stuck client can delay a broadcast by up to one second |
| D7 | Registry is the only module that writes to client sockets and the only one that closes them | Let session threads write and close directly | Prevents interleaved writes and use of closed or reused descriptors; one cleanup path | Session must go through the registry even for private replies |
| D8 | Failed writes call `shutdown`, not `close`; the owning thread closes | Close immediately on failure | A blocked `recv` in another thread is not reliably woken by `close`; `shutdown` wakes it, and one owner closes once | One extra wake-up cycle before cleanup |
| D9 | Client ids never reused within a run | Use slot index as the id | A stale id can never act on a new client in the same slot | A 32-bit counter (wrap-around is beyond any realistic run) |
| D10 | Detached threads plus an active-session counter and condition variable | Joinable threads and `pthread_join` | No join list to manage; the counter tells shutdown when it is safe to exit | A timed wait is needed so shutdown cannot hang |
| D11 | `poll` with a 500 ms timeout in the accept loop | Blocking `accept` plus signal interruption; self-pipe trick | Simple and portable; shutdown latency bounded and tiny | Wakes twice per second when idle (negligible) |
| D12 | Signal handler only sets a `sig_atomic_t` flag | Doing cleanup inside the handler | Only async-signal-safe operations are allowed in handlers; the flag is the safe minimum | None |
| D13 | Framing, command syntax and shared state in three separate modules | One large `server.c` | Each is independently testable; changes stay local; the concurrency argument fits in one file | More files and headers to maintain |
| D14 | Disconnect on over-long line | Discard until the next newline and continue | After an over-long line, line boundaries are untrustworthy; disconnect is simpler and safer | A buggy client loses its connection |
| D15 | Case-insensitive unique nicknames | Case-sensitive | Stops look-alike impersonation | Slightly more comparison code |
| D16 | Tests in Python for end-to-end, C `assert` for units | A C test framework; all tests in C | Spawning 50 clients and injecting faults is quick in Python; unit tests stay in C beside the code | Two languages in the repository (tooling only) |
| D17 | 256 KiB thread stacks | Default 8 MiB stacks | Cuts virtual memory use 32 times; session functions use only a few KiB | Must keep large buffers off the stack (they live in the `linebuf`, about 0.5 KiB) |
| D18 | Compiler-enforced contracts: `WARN_UNUSED` on error-returning functions, `PRINTF_FMT` on variadic ones | Relying on code review alone | Turns NF-4 and format-string bugs into build failures under `-Werror` | Deliberate ignores need an explicit `(void)` cast |
| D19 | No `reg_init`; static mutex initialiser | An init function called from `main` | One fewer function and one fewer thing to forget | None |

### Guiding principles behind the decisions

1. **Make illegal states unrepresentable.** One owner per descriptor, one removal path, one lock: fewer ways to be wrong.
2. **Keep logic that can be pure, pure.** Framing and parsing take bytes in and give results out, so they are tested without a network.
3. **Bound everything.** Clients, line length, nickname length, send time, shutdown time: every resource has a number.
4. **Choose the design you can explain at a whiteboard.** A simpler design you fully understand beats a faster one you cannot defend.

## 9. Verification plan and traceability

Three layers of testing, from fast and narrow to slow and broad. Each requirement in section 2 appears in at least one test.

### 9.1 Unit tests (C, no network, run in under a second)

| Test file | Cases | Requirements |
|---|---|---|
| `test_linebuf.c` | One complete line; two lines in one chunk; a line fed one byte at a time; a line split at every possible position; `\r\n` endings; empty line; exactly 511 bytes plus newline (accepted); 512 bytes with no newline (`LB_TOO_LONG`); partial line kept after extracting a complete one | FR-5, FR-6 |
| `test_proto.c` | Chat; `/nick` valid and invalid (empty, 17 characters, bad characters, trailing spaces); `/who`; `/quit`; unknown command; control characters; leading and trailing spaces; formatting functions never exceed the buffer size | FR-4, FR-5 |
| `test_net.c` | `net_send_all` over a socketpair with a deliberately tiny send buffer and a slow reader proves the loop handles partial writes; timeout produces -1 | FR-7 |

### 9.2 End-to-end tests (Python, real server, real sockets)

| ID | Scenario | Pass condition | Requirements |
|---|---|---|---|
| E1 | Three clients join; one sends a line | The other two receive exactly that line, the sender does not | FR-2, FR-3 |
| E2 | Message order: 3 senders each send 100 numbered lines | Every receiver sees each sender's lines in order, and all receivers see the same global order | FR-3, NF-1 |
| E3 | Send `hello\n` one byte per write with delays | One complete `nick: hello` is delivered | FR-6 |
| E4 | Send five lines in a single write | Five separate messages delivered, in order | FR-6 |
| E5 | Each command: `/nick`, duplicate `/nick`, bad `/nick`, `/who`, `/quit`, `/bogus` | Correct replies and broadcasts | FR-4 |
| E6 | Connect 64 clients, then a 65th | 65th receives `! server full` and is closed; 64 still work | FR-1 |
| E7 | Abrupt reset (socket closed with `SO_LINGER` zero), orderly close, `/quit`, over-long line | Each is removed once; exactly one leave notice each | FR-8, NF-3 |
| E8 | Send 600 bytes with no newline | `! line too long`, connection closed, others unaffected | FR-5, NF-3 |
| E9 | Send bytes containing NUL and control characters | `! invalid characters`, connection stays open | FR-5, NF-3 |
| E10 | A client connects and never reads while another floods the server with large messages | Other clients keep receiving; the stuck client is removed within about 2 seconds | NF-3 |
| E11 | 200 connect/disconnect cycles, 50 at a time | Server's descriptor count (`/proc/<pid>/fd`) returns to its starting value | FR-8, NF-2 |
| E12 | Stress: 50 clients each sending 200 lines concurrently | All lines delivered; no crash; run under ThreadSanitizer reports zero races | NF-1 |
| E13 | Send SIGINT to the server with 10 clients connected | Clients see `* server shutting down` and are closed; exit code 0 within 5 seconds | FR-9 |
| E14 | Server start on an occupied port | Non-zero exit and a clear error message | Rule 3 |

### 9.3 Tool-based verification

| Tool | What it checks | When |
|---|---|---|
| `gcc -Wall -Wextra -Werror -std=c11` | Warnings treated as errors, including unused results | Every build (NF-4, NF-5) |
| AddressSanitizer and UndefinedBehaviorSanitizer (`make asan`) | Buffer overflows, use after free, undefined behaviour | Run all unit and E2E tests (NF-2) |
| ThreadSanitizer (`make tsan`) | Data races and lock misuse | Run E2E stress tests (NF-1) |
| Valgrind (`memcheck`) | Memory leaks and invalid accesses | Final run of E1 to E13 (NF-2) |
| Valgrind (`helgrind`) | Lock-order and race cross-check | Once, on the stress test (NF-1) |
| `strace -f -e trace=network` | Watch the real sequence of `accept`, `recv`, `send` calls | While learning and during interview preparation |
| `tcpdump` or Wireshark on the loopback interface | Observe the handshake, segment splitting, and FIN or RST on disconnect | Once, for the README and interview |
| GDB | Attach to the running server with `gdb -p`; `thread apply all bt` to see every client thread blocked in `recv`; inspect a registry slot; break on `reg_remove` | One documented session (NF-9, R4) |

### 9.4 Traceability: from resume claim to evidence

To claim a bullet in an interview, run the tests in its row and show them passing.

| Resume claim | Requirements | Modules | Tests and tools |
|---|---|---|---|
| R1: multi-client chat server in C on Linux, concurrent connections, broadcast | FR-1, FR-2, FR-3, FR-4, FR-10 | server, session, registry, proto, net | E1, E2, E5, E6, E12 |
| R2: POSIX threads, mutex-protected shared state, clean connect and disconnect | FR-8, FR-9, NF-1, NF-2, NF-8, NF-10 | registry, server, session | E7, E11, E12, E13; ThreadSanitizer, Valgrind |
| R3: framing protocol, partial reads and writes, system-call and dropped-connection error handling | FR-5, FR-6, FR-7, NF-3, NF-4 | linebuf, proto, net | Unit tests; E3, E4, E7, E8, E9, E10 |
| R4: Makefile, GDB, tested with multiple simultaneous clients | NF-5, NF-6, NF-7, NF-9 | Build and test files | `make test`; E6, E12; the documented GDB session |

## 10. Plan of action

The order follows the dependency layers: build bottom-up, so each module is tested before anything depends on it. Total estimate is about 19 hours. Commit to git at the end of every milestone, so a bad edit is one `git checkout` away.

| # | Milestone | Code you write | Tests, docs and self-review you write | Exit criteria | Hours |
|---|---|---|---|---|---|
| M0 (done, 2026-11-29) | Project skeleton | `config.h`, folder layout, `git init`, `Makefile`, `.gitignore`, headers matching section 4's interfaces, header patch (contracts, attributes) | Header standalone-compile check; fresh-clone build | `make` builds an empty project with zero warnings; `make asan` and `make tsan` work; every header compiles standalone | 1 |
| M1 | `log` and `net` | Both modules, completely | `test_net.c` and the `make test` rules for it; self-review against the section 4.2 and 4.3 behaviour lists | A scratch echo server built on `net_listen`, `net_accept`, `net_recv`, `net_send_all` works with `nc`; `test_net.c` passes | 3 |
| M2 | `linebuf` | The module, completely | `test_linebuf.c` with all cases in section 9.1; self-review | All linebuf tests pass under ASan; you can explain `lb_space` and `lb_commit` aloud | 2 |
| M3 | `proto` | The module | `test_proto.c`; self-review | All proto tests pass; no formatting function can overflow its buffer | 1.5 |
| M4 | `registry` | The module, completely, from the six rules in section 4.6 | Self-review against the rules and the race list in 6.5; a stress unit test with many threads calling add, remove and broadcast | Stress test is clean under ThreadSanitizer; every function releases the lock on every path | 3 |
| M5 | `session` and `server` | `session_run`, the accept loop, thread creation, counter and shutdown | Self-review against sections 6.3 to 6.8; first draft of `tests/e2e.py` (E1 to E5) | Three `nc` clients chat live; E1 to E5 pass | 3 |
| M6 | Hardening | Fixes for every failure found | `e2e.py` cases E6 to E14; self-review against the error table in section 7 | E1 to E14 pass; ASan, TSan and Valgrind runs are clean; descriptor count returns to baseline | 3 |
| M7 | Evidence and polish | The GDB session; the optional `chat.c` | README (build, run, protocol, how to test, known limits); the traceability evidence | README complete; GDB session written up; resume claims all backed by passing tests | 2.5 |

**Hours:** the estimates are unchanged from revision 1, but the unit tests, `e2e.py` and README are now written by hand (revision 1 had a coding agent draft them), so expect those parts to run over.

**Order rule:** do not start a milestone until the previous one's exit criteria pass. The most common failure in this kind of project is building the top layer on an unverified lower layer and then debugging all of it at once.

### Working method

No coding agent or code generator is used on this project: every line of code, every test and the README are written by hand, because interviewers will ask you to explain and modify this code. Any other reader (a person, or an AI chat assistant used only for explanation) may review your code against this document, but never writes the modules, and tools always outrank opinions.

**The loop for every module:**

1. Read the module's section in this document.
2. Check the header in `include/`; its comments are the contract.
3. Implement the functions. Compile with `-Wall -Wextra -Werror`.
4. Write the tests from the behaviour description (section 9) and run them.
5. Self-review the file against the module's section: list every rule it breaks and every system call whose result is unchecked.
6. Verify anything doubtful against the man page (`man 2 send`, `man 3 pthread_mutex_lock`) before acting on it.
7. Run the sanitizers (`make asan`, `make tsan`). If a tool and a reviewer disagree, the tool wins.
8. Commit, one commit per logical change.

**Standing rules for the code** (these were the old `.goosehints`; they now live here as a review checklist):

- C11, POSIX only, no external libraries. Flags: `-Wall -Wextra -Werror`.
- Check the return value of every system call. Retry `EINTR` where the design says so.
- Never use `strtok`, `localtime`, `strerror` or other non-thread-safe functions; use the `_r` versions.
- No global state except `g_stop`. No `malloc` on the message path.
- Only `registry.c` may `close()` or `send()` to a registered client's descriptor; everything else goes through `net` and `registry`.
- Work on one module at a time.

### Honesty and interview readiness gate (before applying)

- You can redraw the architecture and the client lifecycle from memory.
- You can explain, without notes, why the registry owns the lock and the closing of descriptors, why `shutdown` is used instead of `close` on a failed write, and how `linebuf` handles a half-received line.
- You can modify a line of the code live, for example change the line limit or add a `/me` command, and rebuild.
- You have used GDB on the real server and can describe what you saw.
- Every claim on the resume matches something that runs. If a claim is not true yet, change the resume rather than the facts.

## 11. Known limitations and upgrade path

The MVP is honest about what it does not do. Knowing these limits, and the fix for each, is itself a strong interview answer.

| Limitation | Effect | Upgrade path | Module affected |
|---|---|---|---|
| Writes happen under the registry lock, so a stuck client delays a broadcast by up to 1 second | Latency spike for everyone during a slow-client event | Per-client outgoing queue with `poll` for `POLLOUT`, or an `epoll` event loop; drop the client if its queue passes a limit | registry, session |
| One thread per client | Practical ceiling in the low thousands, and 256 KiB of stack each | Single-threaded `epoll` loop with non-blocking sockets, using the same `linebuf` and `proto` unchanged | server, session |
| Hard limit of 64 clients | Further clients are refused | Raise `MAX_CLIENTS`, or switch the table to a dynamic structure | config.h, registry |
| Plain text, no encryption or authentication | Anyone on the network can read or impersonate | TLS through OpenSSL; a login step and password check | net, session |
| No escape for chat text that starts with `/` | A user cannot send such a line | Add `//` as an escape, or a `/say` command | proto |
| One global room; no private messages | No targeted delivery | Add `/msg <nick> <text>` using `reg_send_to` by nickname lookup | proto, registry |
| IPv4 only | No IPv6 clients | Use `getaddrinfo` and a dual-stack socket | net |
| No history or persistence | Late joiners see nothing earlier | A ring buffer of the last N lines replayed on join | registry |
| Linux only | Not tested elsewhere | Mostly POSIX; replace `MSG_NOSIGNAL` with `SO_NOSIGPIPE` on other systems | net |

**Best stretch goal for interviews:** after the MVP passes all tests, build the `epoll` variant by replacing only `server.c` and `session.c`. If the module boundaries in this design are right, `net`, `linebuf`, `proto` and `registry` will need little or no change. That demonstrates, with evidence, that the design is modular, and it gives you a concrete answer to "how would you scale this to 10,000 clients?"

### Open decisions

- **OD1: `/who` with a full table.** A full table of 16-character nicknames makes a reply of about 1.2 KiB, which exceeds the 512-byte line limit that `chat`'s `linebuf` enforces. Current choice: build the reply into an `OUT_MAX` buffer and truncate the list, keeping the count exact. Alternative: send one `* ...` line per nickname. Decide before M5, since it affects the `reg_who` contract and test E5.

## Change log

| Date | Change |
|---|---|
| 2026-10-18 | Original specification (PDF) |
| 2026-11-29 | Converted to Markdown. Synced with the M0 header patch: added `IP_STR_MAX`, `WARN_UNUSED`, `PRINTF_FMT` (4.1); `LOGL_*` levels and `strerror_r` (4.2); `proto_fmt_*` return a length or -1 (4.5); removed `reg_init`, added `reg_get_nick` and return-value contracts (4.6); `g_stop` defined in `server.c` (4.8); per-variant build directories (4.10); coupling rule 1 now allows `config.h`; added D18, D19 and OD1; removed Goose from the plan |
| 2026-11-29 (rev 2) | Removed Qwen3-Coder as well as Goose: no coding agent or code generator is used. Plan table columns renamed (all code, tests and docs are hand-written); "Working with a coding model" replaced by "Working method" with the old `.goosehints` rules kept as a review checklist; hour estimates flagged as likely to overrun on tests and docs. Coupling rule 2 clarified: descriptors that never entered the table are closed by the code that rejected them (`session.c` for a full server, `server.c` for a failed thread creation). M0 marked done |
