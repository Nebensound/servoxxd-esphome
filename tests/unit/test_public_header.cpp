#include "../../components/servoxxd/stepper/servoxxd.h"
#include <type_traits>
#include <utility>

namespace esphome {
namespace servoxxd {

// This isolated translation unit must not see even a forward declaration of Layer 4's Command.
using Command = void;

static_assert(std::is_same_v<
              decltype(std::declval<const ConfigData &>().get_update_command_types(std::declval<const ConfigData &>())),
              std::vector<Commandtype>>);

}  // namespace servoxxd
}  // namespace esphome

int main() { return 0; }
