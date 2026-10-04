# wxl-area-loot

Client half of `mod-aoe-loot`'s server-authoritative aggregate loot UI. Requires
`wxl.network` and `wxl.framescript`. The runtime loads this extension from
`Extensions/wxl-area-loot/wxl-area-loot.dll`; its FrameNew UI lives at
`WarcraftXL/AreaLoot/AreaLoot.lua`.

`0x550` is the client request (a strict, at-most-64-byte ASCII command), and
`0x551` carries up to 8 KB of newline-separated state records, each bounded
to 240 bytes. The records retain the
module's `HELLO`, `SET`, `B`, `R`, and `E` snapshot grammar; the transport is
now WXL packets, with no AIO or addon-chat dependency. Only the server
decides which corpses and loot slots are eligible or stores items.
