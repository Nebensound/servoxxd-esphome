# Layer 3: Command Coordination (CommandQueue) - Detailed Specification

**Parent Document:** [02-cpp-interface.md](./02-cpp-interface.md)  
**Status:** 🔵 SPECIFICATION – Layer 3 implementation details

**Navigation:**

- [← Previous: Layer 2 (StepperEngine)](./02b-layer2-stepper-engine.md)
- [← Back to Overview](./02-cpp-interface.md#layer-3-command-coordination-transport-agnostic)
- [→ Next: Layer 4 (Transport)](./02d-layer4-transport.md)

---

## Overview

**Class:** CommandQueue

**Design Pattern:** Command Queue + State Machine

**Role in Architecture:**

- FIFO queue with strict **single-flight execution** (only one command executing at any time)
- Manages command lifecycle: PENDING → EXECUTING → COMPLETED/FAILED/TIMEOUT
- Smart command management (deduplication, coalescing, critical commands)
- Error handling and timeout recovery
- Transport-agnostic (works with ITransport interface and Command enum from Layer 4)

## Responsibilities

- Maintain FIFO queue of transport commands
- Enforce single-flight execution: only one command in EXECUTING state at any time
- Manage command timeouts and retries
- Process responses and errors from transport layer (Layer 4)
- Optimize queue with deduplication, coalescing, and priority handling
- Ensure queue never blocks: failed commands are removed, execution continues
- Bridge between StepperEngine (Layer 2) and ITransport (Layer 4)

## Single-Flight Guarantee

### Core Concept

Only **one command** may be in EXECUTING state at any time. This ensures serialized transport communication and prevents race conditions.

### Execution Guard

**Guard Flag:** `bool execution_guard_` (acts as mutex)

- **true** = command currently executing, waiting for response
- **false** = no command executing, ready to process next

### Execution Flow

1. **`execute_next()`**
   - Check execution guard: if **true**, return immediately (wait for response)
   - If **false** and queue not empty: dequeue next PENDING command
   - Send via transport interface, transition to EXECUTING, start timeout timer
   - Set execution guard = **true**

2. **`on_response_received(data)`**
   - Validate command is executing
   - Process response data
   - Transition command to COMPLETED
   - **Clear execution guard** (CRITICAL: enables next command)
   - Call `execute_next()` to process next pending command

3. **`on_error_received(function_code, exception_code)`**
   - Validate command is executing
   - Log error details
   - Transition command to FAILED
   - **Clear execution guard** (CRITICAL: enables recovery)
   - Call `execute_next()` to continue with next command

4. **`check_timeout()`** (called periodically from ServoXxd::loop())
   - If timeout exceeded:
     - Log timeout with command details
     - Transition command to TIMEOUT
     - **Clear execution guard** (CRITICAL: prevents queue stall)
     - Call `execute_next()` to continue with next command

### Key Invariants

1. Execution guard flag acts as mutex
2. `execute_next()` returns immediately if guard is set
3. Response/error/timeout handlers **must** clear execution guard
4. After completing/failing a command, `execute_next()` is called (tail-recursive processing)
5. Queue never blocks: failed commands discarded, execution continues

## Smart Command Management

### Deduplication

- **Rule:** Duplicate read requests for the same Command ignored if already queued or executing
- **Example:** Two `READ_ENCODER_CARRY` commands → only one queued

### Coalescing

- **Rule:** Successive position/speed commands replace earlier pending ones (keep only newest target)
- **Example:** `MOVE_POSITION_MODE_2(100)` → `MOVE_POSITION_MODE_2(200)` → only 200 queued
- **Benefit:** Reduces queue depth, improves responsiveness
- **Implementation:** Compare Command enum values to detect duplicates

### Critical Commands

- **Rule:** `Command::EMERGENCY_STOP` clears all pending (non-executing) commands before enqueueing
- **Benefit:** Ensures immediate response without waiting for queue

## Error Handling

- **Transport Error:** Mark current command as FAILED, trigger completion callback, advance to next
- **Timeout:** Mark as TIMEOUT, advance to next (no automatic retry - caller's responsibility)
- **Unexpected Response:** Log warning if no command executing
- **Queue Never Blocks:** Failed commands removed, execution continues
- **Error codes passed to StepperEngine** via `on_transport_error(Command, ErrorCode)`

## Integration with ServoXxd::loop()

1. **Periodic Timeout Check:** `check_timeout()` called each loop iteration to detect stuck commands
2. **Tail-Recursive Processing:** After completing/failing a command, `execute_next()` automatically called
3. **Callback Delegation:** Response/error callbacks forwarded to StepperEngine for state machine processing

## Example Command Lifecycle

1. StepperEngine enqueues move_to command → CommandQueue adds to queue (PENDING)
   - Uses Command::MOVE_POSITION_MODE_2 with data encoded by CommandDecoder
2. ServoXxd::loop() calls queue->execute_next()
   → Queue checks guard (false), dequeues command, sends via ITransport, sets guard=true (EXECUTING)
3. Transport response received → queue->on_response_received(Command, data)
   → Forward to StepperEngine, mark COMPLETED, clear guard, call execute_next() for next command
4. If timeout → check_timeout() detects, marks TIMEOUT, clears guard, calls execute_next()


**Integration with Layer 4:**
- CommandQueue holds reference to `ITransport*` interface
- Uses `transport->execute_command(Command, data)` for writes
- Uses `transport->read_command(Command, response)` for reads
- Transport callbacks forwarded to StepperEngine with Command context

## Benefits

- **Reliability:** Single-flight ensures no race conditions
- **Robustness:** Timeout recovery prevents queue stall
- **Performance:** Coalescing and deduplication optimize traffic
- **Simplicity:** StepperEngine doesn't need to manage transport protocol details
- **Testability:** Clear state machine, easy to unit test

---

**Navigation:**

- [← Previous: Layer 2 (StepperEngine)](./02b-layer2-stepper-engine.md)
- [← Back to Overview](./02-cpp-interface.md#layer-3-command-coordination-transport-agnostic)
- [→ Next: Layer 4 (Transport)](./02d-layer4-transport.md)
