# Dynamic Database Design

The project uses persistent CSV files. This keeps the web application portable and dependency-free while providing dynamic records that survive server restarts.

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
- `payment_method` — `CASH`, `MPESA`, `CARD`, or `FREE`.
- `payment_reference` — verified provider reference for M-Pesa/card payments.
- `receipt_number` — sequential MMU receipt identifier.
- `vat_amount` — VAT component included in the gross charge.

Older transaction rows with only the original seven columns remain readable. New completed transactions are written with all audit columns.

## RATES

`data/rates.csv` stores the free-time limit, two paid-band limits, each band amount, and the VAT percentage. Management can update these values in the web dashboard; the new schedule is persisted and applies to subsequent exits. Fees are treated as VAT-inclusive, with the VAT component recorded per transaction.

## Integrity rules

1. A slot cannot be allocated if it is occupied.
2. An active registration cannot be admitted twice.
3. A vehicle must exist in ACTIVE_VEHICLES before exit can be processed.
4. Exit calculation alone does not release a slot; paid exits require payment confirmation.
5. A confirmed payment writes the transaction audit record, then makes the slot available and removes the active vehicle.
6. The completed transaction remains in TRANSACTIONS.
7. State-changing operations save the affected files.
8. A mutex protects in-memory state during concurrent HTTP requests.

## Payment integration boundary

The current prototype records operator confirmation and an externally verified M-Pesa/card reference; it does not contact a payment gateway or bank terminal. Production use requires authenticated operator access and a provider integration that verifies payment callbacks before the confirmation endpoint can be trusted.
