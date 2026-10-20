// Built as its own target linked to coop_net only, with no transport library
// on its include path (tests/CMakeLists.txt). Stands for game-side code: it
// sees only coop::net types, and network input reaches it only through poll()
// on its own thread.
#include "coop/net/fake_network.h"
#include "coop/net/transport.h"

#include <vector>

namespace coop::test
{
// One simulation tick's network step: drain events, count messages.
int game_side_drain(coop::net::Transport& t, std::vector<coop::net::Event>& scratch)
{
    scratch.clear();
    t.poll(scratch);
    int messages = 0;
    for (const coop::net::Event& e : scratch)
        if (e.type == coop::net::EventType::Message)
            ++messages;
    return messages;
}
} // namespace coop::test
