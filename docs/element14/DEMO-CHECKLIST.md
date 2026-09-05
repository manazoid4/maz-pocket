# Element14 Physical Demo Gate

A competition-facing feature is recordable only after three consecutive successful runs on the real M5Stack Cardputer ADV.

## Boot

- [ ] Firmware boots normally
- [ ] Keyboard input works
- [ ] Wi-Fi connects
- [ ] MAZ Core becomes reachable

## CALL — run 3 times

For each run:

- [ ] Hold/release Space records audio
- [ ] Audio is transmitted
- [ ] Speech is transcribed
- [ ] Selected route returns a response
- [ ] Response is shown on the Cardputer
- [ ] Spoken reply works when enabled
- [ ] No freeze, retry storm or unexplained generic error

## Routing

- [ ] LOCAL succeeds when local inference is available
- [ ] Configured online route succeeds when available
- [ ] AUTO follows the configured bounded fallback policy
- [ ] Unavailable provider produces a useful bounded error
- [ ] Route shown by UI matches the route actually used

## CAPTURE

- [ ] Brain Dump starts
- [ ] Brain Dump stops cleanly
- [ ] Result is saved and accessible
- [ ] Repeat 3 times

Teach Demo is optional. Include it only after the same three-run gate passes.

## AGENTS -> PLAN

- [ ] Project can be selected
- [ ] Task can be submitted
- [ ] Current project evidence is used
- [ ] Plan returns successfully
- [ ] PLAN does not execute work
- [ ] Repeat 3 times

## CONTROL

- [ ] Wi-Fi state is truthful
- [ ] MAZ Core state is truthful
- [ ] Active route/provider state is truthful
- [ ] Pairing state is truthful if shown
- [ ] Any RSSI/latency/traffic values shown are measured rather than invented

## Recording gate

Do not record the final competition video until all required sections above pass. If an optional feature repeatedly fails, hide/demote it instead of expanding the sprint to rescue it.
