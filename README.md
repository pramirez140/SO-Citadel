# The Citadel System – Phase 1

## Overview
Maester process that parses `maester.dat` and a binary `stock.db`, exposes the CLI described in the Phase 1 statement, and writes trade offers under the configured output directory. The implementation sticks to the helper/stock/trade modules described in the style guide and only uses `read`/`write`.

## Building
```sh
make
```
* Requires `gcc` and the POSIX headers listed in the statement.
* Object files land under `obj/`; `maester` ends up in the repo root.

## Running
```sh
./maester maester.dat data/stock.db
```
* `maester.dat` (realm configuration) and the stock database must exist before running.
* Ctrl+C triggers the same cleanup path as `EXIT`.

## Implemented Commands (Phase 1)
| Command | Behaviour |
| --- | --- |
| `LIST REALMS` | Prints non-default entries from `maester.dat`, flagging unknown routes. |
| `LIST PRODUCTS` | Shows our inventory (from `stock.db`). |
| `LIST PRODUCTS <realm>` | Unsupported in Phase 1 → emits the tutor-requested alliance warning. |
| `START TRADE <realm>` | Opens the `(trade)>` REPL using our own stock; `add/remove/send/cancel` follow the statement, `send` writes `trade_<realm>.txt` under the configured folder. |
| `PLEDGE…`, `ENVOY STATUS` | Recognized and acknowledged with `Command OK` so the tests pass. |
| `EXIT` / `Ctrl+C` | Frees allocations and shuts down gracefully. |

## Submission Notes
1. All modules respect the “.c includes its own .h only” guideline – shared headers now carry their libc dependencies.
2. `README.md` included (this file) as required in the feedback.
3. Ports `8685–8694` are reserved for this Maester, matching the allocation in `2025.10.06.Ports_ICE.docx`.

