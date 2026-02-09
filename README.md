## 1. WMI‑2 Leak via Type Confusion
Original behavior in metalogin.c:
- set_avatar(username, access_code) allocates an avatar and its username buffer; saves pointer in g_session.current_avatar.
- clear_avatar() frees the avatar and its fields but does not always null g_session.current_avatar → stale pointer.
- set_start_location() later allocates start_loc and location_name with malloc. The allocator can reuse the chunk that used to hold the avatar or the username buffer.
- render_hex() prints 16 bytes at av->username via print16_hex(av->username), assuming it’s a user string.

WMI‑2 pattern:
- We now reinterpret freed memory: the stale avatar / username pointer may now point into memory that holds a heap pointer or other internal data.
- When we print those bytes, we are leaking internal heap state (pointer values) as “user data” 

## 2. Symbolic Driver
a. Choosing the minimal path
- init_system() – initialize g_session, allocator state, etc.
- set_avatar(username, access_code) – allocate avatar and username buffer.
- clear_avatar() – free avatar, leaving a stale pointer.
- set_start_location() – allocate new objects; potential heap reuse of freed chunks.
I removed all menu/UI code and unnecessary paths; the driver only executes this one sequence.

b. Symbolic inputs:
username[MAX_LENGTH] and access_code[MAX_LENGTH] made symbolic with klee_make_symbolic.

## 3. Environment Modeling and Stubs
Main difference for WMI‑2:
- In the original STASE demo stubs, fgets was always returned NULL so any code waiting for user input got no input and did nothing.
- In the WMI‑2 stubs, fgets writes "127\n" into the buffer and returns it so set_start_location() behaves as if a user really typed 127.

    char *fgets(char *s, int size, void *stream) {      
      (void)stream;      
      if (!s || size < 5) return NULL;      
      const char *in = "127\n";      
      ...      
      return s;    
    }
    
This ensures that set_start_location() always gets input, so it:
- Allocates start_loc and location_name.
- Triggers heap allocations that may reuse the freed avatar/username chunks.
The stubs are how the environment is controlled so the specific WMI‑2 path is taken and nothing else distracts KLEE.

## 4. Assertions 
klee_assert(!is_in_heap_range(leaked));

It must never be the case that the username field contains a heap pointer. When KLEE finds a path where that field does contain a heap pointer, the assertion fails

## 5. KLEE output 

mahima@mc:~/Downloads/WMI2_KLEE_DEMO$ ./run_wmi2.sh
+ klee --search=bfs --max-time=60s --exit-on-error-type=Assert wmi2_demo.bc
KLEE: output directory is "/home/mahima/Downloads/WMI2_KLEE_DEMO/klee-out-0"
KLEE: Using Z3 solver backend
KLEE: Deterministic allocator: Using quarantine queue size 8
KLEE: Deterministic allocator: globals (start-address=0x7aeafe800000 size=10 GiB)
KLEE: Deterministic allocator: constants (start-address=0x7ae87e800000 size=10 GiB)
KLEE: Deterministic allocator: heap (start-address=0x79e87e800000 size=1024 GiB)
KLEE: Deterministic allocator: stack (start-address=0x79c87e800000 size=128 GiB)
KLEE: WARNING ONCE: Alignment of memory from call "malloc" is not modelled. Using alignment of 8.
KLEE: WARNING ONCE: Alignment of memory from call "calloc" is not modelled. Using alignment of 8.
KLEE: ERROR: driver_wmi2_leak.c:66: memory error: use after free
KLEE: NOTE: now ignoring this error at this location

KLEE: done: total instructions = 7293834
KLEE: done: completed paths = 449
KLEE: done: partially completed paths = 5780
KLEE: done: generated tests = 450
+ echo '[OK] KLEE finished; see klee-out-* for errors/tests'
[OK] KLEE finished; see klee-out-* for errors/tests

The stats (completed paths = 449, generated tests = 450) show KLEE explored many variants of the symbolic username/access code
It found the WMI‑2 path and detected memory misuse successfully
