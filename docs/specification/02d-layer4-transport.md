# Layer 4: Transport Abstraction - Detailed Specification

**Parent Document:** [02-cpp-interface.md](./02-cpp-interface.md)  
**Status:** 🔵 SPECIFICATION – Layer 4 design and contracts

**Navigation:**
- [← Previous: Layer 3 (CommandQueue)](./02c-layer3-command-queue.md)
- [← Back to Overview](./02-cpp-interface.md#layer-4-transport-abstraction)

---

## Overview

**Components:** 
1. `Command` enum - Type-safe command identifiers (0x30-0xFF)
2. `ITransport` interface - Protocol-agnostic transport contract
3. `CommandDecoder` - Encode/decode command payloads
4. `ModbusTransport` / `SerialTransport` - Protocol implementations

**Design Pattern:** Strategy (ITransport) + Codec (CommandDecoder)

**Key Responsibilities:**
- Abstract protocol details (Modbus-RTU, Serial FA/FB)
- Provide unified Command-based API for upper layers
- Manage framing, addressing, CRC calculation
- Handle request-response state machine (half-duplex)

**Design Goals:**
1. Transport agnostic upper layers (Command enum only)
2. Protocol isolation (Modbus/Serial details hidden)
3. Type safety (no raw register addresses)
4. Separation of concerns (Transport = framing, Codec = data encoding)

---

## Component 1: Command Enum

**Purpose:** Type-safe hardware command identifiers. Maps 1:1 to command codes (0x30-0xFF).

**Location:** `servoxxd_commands.h`

```cpp
enum class Command : uint8_t {
  // Read Commands
  READ_ENCODER_CARRY = 0x30,
  READ_ENCODER_ADDITION = 0x31,
  READ_CURRENT_SPEED = 0x32,
  READ_PULSE_COUNT = 0x33,
  READ_IO_STATUS = 0x34,
  READ_MOTOR_STATUS = 0x3A,
  READ_HOMING_STATUS = 0x3B,
  READ_PROTECTION_STATUS = 0x3E,
  
  // Configuration Commands
  SET_WORKING_CURRENT = 0x44,
  SET_SUBDIVISION = 0x84,
  SET_WORK_MODE = 0x82,
  SET_HOME_PARAMS = 0x4A,
  
  // Movement Commands (Position Modes)
  MOVE_POSITION_MODE_1 = 0xFD,  // Same code for STOP (pulses=0)
  MOVE_POSITION_MODE_2 = 0xFE,  // Same code for STOP (position=0)
  MOVE_POSITION_MODE_3 = 0xF4,
  MOVE_POSITION_MODE_4 = 0xF5,
  
  // Movement Commands (Speed Mode)
  MOVE_SPEED_MODE = 0xF6,       // Same code for STOP (speed=0)
  
  // Special Commands
  EMERGENCY_STOP = 0xF7,
  ENABLE_MOTOR = 0xF3,
  START_HOMING = 0x9A,
  CALIBRATE_ENCODER = 0x80,
  RESTART_CONTROLLER = 0x3F,
  RELEASE_PROTECTION = 0x0E,
};
```

**Key Insight:** Same command code can represent different operations (MOVE vs STOP) based on data payload. Upper layers use CommandDecoder to differentiate.

---

## Component 2: ITransport Interface

**Purpose:** Protocol-agnostic contract for command execution.

**Location:** `servoxxd_transport.h`

```cpp
class ITransport {
 public:
  virtual ~ITransport() = default;
  
  // Execute write command with data payload
  virtual Result execute_command(Command cmd, const std::vector<uint8_t>& data = {}) = 0;
  
  // Execute read command and retrieve response
  virtual Result read_command(Command cmd, std::vector<uint8_t>& response) = 0;
  
  // Check if transport is busy (waiting for response)
  virtual bool is_busy() const = 0;
  
  // Process state machine, timeouts, incoming data
  virtual void update() = 0;
};

struct Result {
  bool success;
  ErrorCode error_code;
};

enum class ErrorCode {
  OK, TIMEOUT, PROTOCOL_ERROR, DEVICE_ERROR, INVALID_RESPONSE, BUSY
};
```

**Contracts:**
- `execute_command()` / `read_command()`: Non-blocking, return immediately
- Precondition: `is_busy() == false`
- Responses processed asynchronously in `update()`
- Callbacks registered by CommandQueue

**State Machine:**
```
IDLE → (execute/read) → WAITING_RESPONSE → (response/timeout) → IDLE
```

---

## Component 3: CommandDecoder

**Purpose:** Encode parameters to bytes (transmission) and decode bytes to structured types (responses).

**Location:** `servoxxd_command_codec.h` / `.cpp`

**Design:** Pure static functions (no state)

### Key Methods

```cpp
class CommandDecoder {
 public:
  // === Movement Encoders ===
  static std::vector<uint8_t> encode_move_position_mode_2(
    int32_t position, uint16_t speed, uint8_t accel);
  static std::vector<uint8_t> encode_stop_position_mode_2(uint8_t decel);
  
  static std::vector<uint8_t> encode_move_speed_mode(
    uint16_t speed, uint8_t accel, Direction dir);
  
  // === Configuration Encoders ===
  static std::vector<uint8_t> encode_set_working_current(uint16_t mA);
  static std::vector<uint8_t> encode_set_subdivision(uint8_t microsteps);
  static std::vector<uint8_t> encode_enable_motor(bool enable);
  
  // === Response Decoders ===
  static int16_t decode_current_speed(const std::vector<uint8_t>& data);
  static int32_t decode_pulse_count(const std::vector<uint8_t>& data);
  static EncoderValue decode_encoder_carry(const std::vector<uint8_t>& data);
  static MotorStatus decode_motor_status(const std::vector<uint8_t>& data);
  static ProtectionStatus decode_protection_status(const std::vector<uint8_t>& data);
};

enum class Direction { CW = 0, CCW = 1 };
enum class WorkMode { CR_OPEN = 0, SR_VFOC = 5 /* ... */ };

struct EncoderValue { int32_t carry; uint16_t value; };
struct MotorStatus { enum State { STOP, MOVING, HOMING /* ... */ }; State state; };
struct ProtectionStatus { bool protected_state; };
```

**Error Handling:**
- Encoders: Validate ranges, return empty `{}` on error
- Decoders: Check buffer size, return default values on error

**Byte Ordering:** Big-endian (MSB first)

---

## Component 4: ModbusTransport Implementation

**Purpose:** Implement ITransport using Modbus-RTU protocol.

**Location:** `servoxxd_modbus_transport.h` / `.cpp`

### Design Outline

```cpp
class ModbusTransport : public ITransport {
 public:
  ModbusTransport(modbus::ModbusClientDevice* device);
  
  Result execute_command(Command cmd, const std::vector<uint8_t>& data) override;
  Result read_command(Command cmd, std::vector<uint8_t>& response) override;
  bool is_busy() const override;
  void update() override;
  
 private:
  modbus::ModbusClientDevice* device_;
  State state_;  // IDLE, WAITING_RESPONSE
  Command pending_command_;
  uint32_t timeout_start_ms_;
  std::function<void(Command, const std::vector<uint8_t>&)> response_callback_;
  std::function<void(Command, ErrorCode)> error_callback_;
};
```

**Protocol Mapping:**
- Command enum value → Modbus register address (direct mapping)
- Read: Function 0x04 (Read Input Registers)
- Write: Function 0x06 (Single) or 0x10 (Multiple)
- CRC16 calculated per Modbus standard

**Integration:** Wraps ESPHome's `modbus::ModbusClientDevice` (ESPHome 2026.8.2 or newer).
Uses typed read/write request helpers and checks their queue-acceptance result.
The Layer 1 bridge forwards `on_response(request_pdu, response_pdu)` and
`on_error(request_pdu, exception_code)` to the transport. PDU spans are only valid
during the callback; the transport copies response payloads that outlive it.

---

## Component 5: SerialTransport (Future)

**Status:** Placeholder for future implementation

**Protocol:** Serial FA/FB frames with CRC8

```cpp
class SerialTransport : public ITransport {
  // Similar structure to ModbusTransport
};
```

---

## Usage Examples

### Layer 2 (StepperEngine)

```cpp
void StepperEngine::move_to(Position target, Speed speed, Acceleration accel) {
  auto data = CommandDecoder::encode_move_position_mode_2(
    target.to_pulses(), speed.to_motor_units(), accel.to_motor_units());
  
  transport_->execute_command(Command::MOVE_POSITION_MODE_2, data);
}

void StepperEngine::update_speed() {
  std::vector<uint8_t> response;
  if (transport_->read_command(Command::READ_CURRENT_SPEED, response).success) {
    int16_t rpm = CommandDecoder::decode_current_speed(response);
    current_speed_ = Speed::from_rpm(rpm);
  }
}
```

### Layer 1 (ServoXxd)

```cpp
void ServoXxd::set_working_current(float mA) {
  auto data = CommandDecoder::encode_set_working_current(static_cast<uint16_t>(mA));
  transport_->execute_command(Command::SET_WORKING_CURRENT, data);
}
```

---

## Command Reference (Excerpt)

| Command | Code | Modbus | Serial | Description |
|---------|------|--------|--------|-------------|
| READ_ENCODER_CARRY | 0x30 | `04 0030 0002` | `FA 01 30 [CRC8]` | Read encoder |
| READ_CURRENT_SPEED | 0x32 | `04 0032 0001` | `FA 01 32 [CRC8]` | Read RPM |
| SET_WORKING_CURRENT | 0x44 | `06 0044 [data]` | `FA 01 44 [hi][lo] [CRC8]` | Set current |
| MOVE_POSITION_MODE_2 | 0xFE | `10 00FE 0004 [8B]` | `FA 01 FE [8B] [CRC8]` | Absolute move |
| EMERGENCY_STOP | 0xF7 | `06 00F7 0001` | `FA 01 F7 [CRC8]` | Halt |

**Notes:** Function codes: 04=Read, 06=Write Single, 10=Write Multiple. All values big-endian.

---

## Benefits

1. **Clean Separation:** Protocol details isolated from movement logic
2. **Testability:** Mock ITransport for unit tests
3. **Extensibility:** Add protocols without changing Layers 1-3
4. **Type Safety:** Command enum prevents errors
5. **Maintainability:** Codec centralizes protocol knowledge

---

**Navigation:**
- [← Previous: Layer 3 (CommandQueue)](./02c-layer3-command-queue.md)
- [← Back to Overview](./02-cpp-interface.md#layer-4-transport-abstraction)
