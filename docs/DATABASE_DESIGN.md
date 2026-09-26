# Dynamic Database Design

The project uses a persistent file-based database made of three CSV tables. This keeps the web application portable and dependency-free while providing dynamic records that survive server restarts.

## SLOTS

- `id` — unique parking-slot identifier (primary key concept).
- `occupied` — current occupancy state.
- `plate` — registration occupying the slot, if any.

## ACTIVE_VEHICLES

- `plate` — active vehicle registration and logical key.
- `owner` — driver/owner name.
- `slot` — allocated parking slot.
- `arrival` — timestamp used for fee calculation.

## TRANSACTIONS

- `plate` — vehicle registration.
- `slot` — slot used.
- `arrival` — entry timestamp.
- `departure` — exit timestamp.
- `duration_minutes` — calculated duration.
- `fee` — parking charge in KSh.
- `payment_status` — `FREE` or `PAID`.

## Integrity rules

1. A slot cannot be allocated if it is occupied.
2. An active registration cannot be admitted twice.
3. A vehicle must exist in ACTIVE_VEHICLES before exit can be processed.
4. Successful exit makes the slot available and removes the active vehicle.
5. The completed transaction remains in TRANSACTIONS.
6. State-changing operations save the affected files.
7. A mutex protects in-memory state during concurrent HTTP requests.
