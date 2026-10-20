# Sessions and authentication

How a remote player becomes a session bound to a canonical player (S04-T04,
[validation](validation/S04-T04.md)). Code: `protocol/include/coop/session/`
(`crypto.h`, `auth.h`, `server.h`, `client.h`), `protocol/src/session/`,
`protocol/tools/coop_keytool.cpp`. Wire messages:
[wire format v1](network-protocol.md#wire-format-v1). Liveness, resume and
token rotation are S04-T05.

## Supported deployment (initial)

- One dedicated campaign server, run by one of the players (the host) or on a
  machine they control, listening on one UDP port. Clients connect by
  `address:port` (S04-T01: no relay, no Steam, no NAT traversal). Over the
  internet the host forwards that UDP port and allows it through the firewall.
  Only one-host loopback has been tested so far (plan.md R06: no claim of
  internet reachability yet).
- A closed group: the host lists every player allowed into the campaign in a
  roster. No accounts, no public matchmaking, no paid identity or relay
  service (those would need scope approval, plan.md R06).

## Credentials

| Who | Holds | Where | Secret? |
|---|---|---|---|
| Player | a P-256 key pair and a random 128-bit player id | key file made by `coop-keytool new-player`; encrypted with DPAPI for that Windows user (`CryptProtectData`), so it opens only for them on that machine | yes: never sent, never logged, wiped from memory after use |
| Host | the roster: one line per player, `player <id> <public key> <display name>` | a text file next to the server's configuration | no: public keys and ids only; a leaked roster lets nobody in |

Joining a campaign: the player runs
`coop-keytool new-player --name "<name>" --key <file>` once and sends the
printed roster line (not the key file) to the host, who adds it to the roster.
`coop-keytool show` prints the line again. Keys are not tied to a campaign; a
host may list the same player in several rosters.

Cryptography is Windows' own: BCrypt ECDSA P-256 for signatures, SHA-256, the
system RNG (`BCryptGenRandom`), and DPAPI. BCrypt is already loaded by the GNS
DLL (S04-T01), so nothing new ships.

## Handshake

```
client                                   server
  Hello {player id, client nonce,  ─────▶ check major / build / content → Reject 1/2/3
         build id, content digest}
                                   ◀───── AuthChallenge {server nonce, campaign id}
  AuthProof {sig(transcript)}      ─────▶ verify with the roster key for that player id
                                           → Reject AuthFailed (5) if absent or invalid
                                           → slot: the player's own, or a free one
                                             → Reject ServerFull (4) if none
                                   ◀───── Welcome {campaign, session, player, party,
                                                   slot, slot generation, epoch, ...}
```

The transcript is SHA-256 over a fixed label, the campaign id, both nonces,
the player id, the build id, the content digest and the client's version.
Every unknown player id is still challenged, so a stranger cannot tell which
ids exist.

What it stops:

- **Strangers:** without a listed key nothing past the challenge is possible,
  and only Hello and AuthProof are accepted before the Welcome.
- **Eavesdroppers:** GNS encrypts every connection (AES-GCM).
- **Impersonation:** the other player's key does not verify for your player
  id; a proof names its player id.
- **Replay:** a recorded proof fails against a new server nonce.
- **Wrong campaign or build:** the transcript binds both. A client can pin the
  campaign (`ClientConfig::expected_campaign`) and refuses to sign for another.

**What it does not stop (stated limit).** GNS's encryption is unauthenticated
(no certificates, S04-T01) and the proof is not bound to the GNS session keys,
which the library does not expose. So an attacker on the network path who
relays a live handshake between a real client and the server ends up holding
the session it relayed. Fixing that needs either GNS certificates (open-source
GNS takes its root CA key at compile time,
`steamnetworkingsockets_certstore.cpp:271-317`, so each host would rebuild the
library) or application-level encryption keyed by this handshake. Recorded
for release qualification (G11, R06).

## Join rules (`session::Server`)

- **Order:** Hello, then AuthProof, then the session. Anything else before the
  Welcome closes the connection (protocol violation, 1003). So does a second
  Hello, any server-only type, or anything that fails `decode()` or the
  stream guard.
- **Handshake timeout:** 5 s by default. An idle or half-finished handshake
  loses its connection (1101), so it cannot hold one of the transport's
  places.
- **Slots:** `max_players` = 2. A player keeps their slot (and its generation)
  while disconnected, so a third player cannot take it. A player who connects
  again gets the same slot. If their old connection still looks alive, the
  new authenticated one **replaces** it: the old one is closed with 1103 and
  the game sees `Left{replaced}` then `Joined` (S04-T01 finding 2). Only the
  server releases a slot (`release`, 1104). The next owner gets generation +1,
  so references to the old binding go stale.
- **Rate limits:**
  - At most 6 handshakes per remote address per minute; beyond that a new
    connection is closed at once (1102).
  - After the Welcome, a token bucket per connection and channel: heartbeats
    10/s (burst 20), actions 30/s (60), snapshot acks 60/s (120), inputs 120/s
    (240). Over the limit, a reliable channel closes the connection (1102); a
    transient one drops the message.
  - Before any of this, the transport caps open connections at
    `max_connections` (4 by default: two players plus two handshakes) and
    refuses the rest with 1001.
- **No privileged role:** game code receives `SessionEvent`s. A message event
  names the player its connection is bound to; no message type names the
  acting player, so a client cannot act as anyone else. Remote players never
  become the engine's in-process bridge client (`bLocal`,
  [participant identity](participant-identity.md)). S05-T01 attaches each
  session to its participant slot.
- **Logging:** diagnostics hold addresses, slots and reasons only. No key,
  nonce, signature or token is ever formatted. The resume token is stored only
  as its SHA-256 (for S04-T05).

## Build and content identity

The Hello's `build_id` and `content_digest` must equal the server's.

- `build_id`: SHA-256 of the engine source pin and the ordered engine patch
  series (`docs/source-manifest.json`: upstream revision plus each patch's
  SHA-256, in `series` order) and the protocol major. Two builds made from the
  same pin and patches match; any engine change makes them differ.
- `content_digest`: SHA-256 of the sorted (path, SHA-256) list of the game
  content both sides load: the installed `gamedata` archives and the co-op
  scripts and configs.

Computing both at start-up belongs to S05-T01, which first runs the session
layer inside the engine. The tests use fixed digests.

## Close codes (transport range 1100–1999)

| Code | Meaning |
|---|---|
| 1100 | Rejected: a Reject message was sent first |
| 1101 | Handshake timeout |
| 1102 | Rate limited |
| 1103 | Replaced by a newer connection of the same player |
| 1104 | Released by the server |
