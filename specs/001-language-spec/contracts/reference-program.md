# Contract: reference program directory (`programs/NN-name/`)

Decisions: D-25 (fault injection lives outside the source), D-26 (formats).

## Files

| File | Required | Content |
|---|---|---|
| `main.qx` | yes | The program. Its entry point is `(define-function main [input] …)`. It uses only the forms in [core-forms.md](./core-forms.md). Call sites that conditions target are wrapped in `(tag :name expr)`. |
| `conditions.qx` | yes | A single quux map with the injected conditions (schema below). |
| `expected.qx` | yes | A single quux vector of assertions (forms below). |
| `README.md` | yes | Explains what the program tests in the design (2–6 lines), the spec sections and decisions it exercises, and any H-n it informs. |
| `actors.qx` | 04, 10, 12 | An actor version using the hypothetical library (D-24). |

## `conditions.qx` schema

```quux
{:input   <any quux value passed to main>            ; optional
 :delays  {:tag-name milliseconds, …}                           ; delay before the tagged expr completes
 :fail    {:tag-name {:kind :user :after-milliseconds 50}, …}   ; tagged expr fails
 :nodes   [{:name :a} {:name :b :crash-after {:completed 4}}]   ; programs 11–12
 :network {:drop [{:from :a :to :b :message :acknowledgement :occurrence 7}]           ; message loss
           :latency-milliseconds 5}
 :files   {"lines.txt" {:generate :numbered-lines :count 10000}}
 :bounds  {:wall-clock-milliseconds 1000}}                            ; generous upper bound for the run
```

Unknown keys are an error in the harness, so typos never silently disable an injection.

Harness conventions:

- `(tag :name expression)` evaluates `expression` and returns its value. When conditions are
  loaded, an entry in `:delays` waits that many milliseconds **before** the expression is
  evaluated, and an entry in `:fail` raises an error of the given `:kind` after
  `:after-milliseconds`, **instead of** evaluating it. A tag applies every time its expression
  is evaluated. Injected waits are cancellation points, like `sleep`.
- A tag evaluated inside a future, directly or through the functions it calls, names the
  innermost future that evaluates it, for `future-state` and `future-error`. A tag around an `agent` or `make-stream` call names that agent or stream for
  `agent-final` and `maximum-ahead`. When a tag is evaluated several times, the assertion must
  hold for every instance.
- `:generate :numbered-lines` writes the lines `1`, `2`, … `:count`, one decimal number per
  line.
- `same-as-sequential` runs `sequential-main` with the same input and compares its result with
  the result of `main` using `equal?`. Programs that use it define both functions.
- `never` evaluates its predicate form in the program's namespace after every committed agent
  update and at the end of the run.
- With `:nodes`, the harness creates one simulated node per entry (Phase 1) or one container
  per entry (Phase 5), and runs `main` on the **first** node listed.
- `:crash-after {:completed n}` crashes the node at the moment its `n`-th `spawn-on` task
  completes. That result is still delivered; no later result leaves the node. From then on,
  every message to the node is lost, and its pending futures fail with `:node-unreachable`.
- `:network :drop` loses the matching message: `:message :acknowledgement` matches the replies to
  agent updates, and `:occurrence n` selects the `n`-th such message between the two nodes.
  In simulation, the sender's pending future fails with `:transport` as soon as its message is
  dropped. On real networks, the deadline after which a missing reply becomes `:transport` is
  decided in Phase 5a.
- `(applied-once identifier)` holds when the update sent with `update-once` and that operation
  identifier was applied exactly once, whatever the number of sends.

## `expected.qx` assertion forms

The value is a vector. Each element is one assertion, and all of them must hold.

```quux
[(returns <value>)                       ; main's result is equal? to value
 (returns-satisfying <function-form>)                ; (function-form result) is truthy
 (raises <kind>)                         ; main raises an error of kind
 (raises-one-of [<kind> …])              ; for racing failures (D-11, H9)
 (suppressed-includes <kind>)            ; primary error carries a suppressed error of kind
 (future-state <tag> <:filled|:failed>)  ; final state of the future created at tag
 (future-error <tag> <kind>)             ; e.g. (future-error :slow :cancelled)
 (agent-final <tag> <value>)             ; final committed state of the agent at tag
 (never <predicate-form>)                     ; invariant checked at each observation point
 (maximum-ahead <tag> <n>)               ; stream produced-but-undemanded ≤ n
 (elapsed-under <milliseconds>)                    ; wall-clock bound (generous; ordering, not speed)
 (elapsed-at-least <milliseconds>)                 ; proves parallelism vs serial sum, with margin
 (output-lines [<string> …])             ; printed lines, in order
 (same-as-sequential)                    ; parallel-transform/fork-join result equal? to sequential
 (deterministic <runs>)                  ; result and hash identical across n runs
 (applied-once <operation-identifier>)]                 ; program 12 idempotence
```

Rules:

- No assertion may depend on an exact time.
- Timing assertions use bounds with a margin of at least 2× over the injected delays.
- When several failures race, use `raises-one-of`.

## Example (program 03, abridged)

```quux
;; main.qx
(define-function main [_]
  (with-scope [s]
    (let-parallel [a (tag :failing (sequence (sleep 1000) :never))
                  b (tag :slow    (sequence (sleep 10000) :never))]
      [a b])))

;; conditions.qx
{:fail {:failing {:kind :user :after-milliseconds 50}} :bounds {:wall-clock-milliseconds 1000}}

;; expected.qx
[(raises :user)
 (future-error :slow :cancelled)
 (elapsed-under 1000)]
```
