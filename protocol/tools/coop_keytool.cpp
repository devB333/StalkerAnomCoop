// coop-keytool: player credentials for co-op sessions (S04-T04).
//
//   coop-keytool new-player --name "<display name>" --key <file>
//       Makes a key pair and player id, writes the key file (DPAPI, opens only
//       for this Windows user) and prints the player's roster line.
//   coop-keytool show --name "<display name>" --key <file>
//       Prints the roster line for an existing key file.
//
// The roster line holds public data only; send it to the server's host, who
// adds it to the roster file. Never send the key file.
#include "coop/session/auth.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace coop::session;

int main(int argc, char** argv)
{
    std::string command = argc > 1 ? argv[1] : "", name, path;
    for (int i = 2; i + 1 < argc; i += 2)
    {
        if (!std::strcmp(argv[i], "--name"))
            name = argv[i + 1];
        else if (!std::strcmp(argv[i], "--key"))
            path = argv[i + 1];
    }
    if ((command != "new-player" && command != "show") || name.empty() || path.empty())
    {
        std::fprintf(stderr, "usage: coop-keytool new-player|show --name \"<display name>\" --key <file>\n");
        return 2;
    }
    RosterEntry e;
    e.display_name = name;
    std::string error;
    if (command == "new-player")
    {
        if (std::ifstream(path).good())
        {
            std::fprintf(stderr, "refusing to overwrite %s\n", path.c_str());
            return 1;
        }
        PrivateKey key;
        if (!generate_key(key, e.key))
        {
            std::fprintf(stderr, "key generation failed\n");
            return 1;
        }
        e.player_id = random_uuid();
        if (!save_key_file(path, e.player_id, key, &error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
    }
    else
    {
        PrivateKey key;
        if (!load_key_file(path, e.player_id, key, e.key, &error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
    }
    Roster check;
    if (!check.add(e, &error))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("%s\n", Roster::line(e).c_str());
    return 0;
}
