// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software. See AUTHORS and LICENSE for more
// information on the copying conditions.

#include "../nettests/utils_impl.hpp"

namespace mk {
namespace nettests {

ErrorOr<std::deque<std::string>>
process_input_filepaths(const bool &needs_input,
                        const std::list<std::string> &input_filepaths,
                        const std::string &probe_cc, const Settings &options,
                        Var<Logger> logger) {
    return process_input_filepaths_impl(needs_input, input_filepaths, probe_cc,
                                        options, logger);
}
} // namespace nettests
} // namespace mk
