# MMU Modern Parking System

Web-based parking management prototype written in C++17 with a responsive browser dashboard and a separate live entrance display. Records persist in CSV files under `data/`.

## Run

On Windows, run `build.bat` and then `run.bat`. Open `http://localhost:8080` for the operator dashboard or `http://localhost:8080/board` on an entrance display. The board and dashboard update occupancy every three seconds.

## Features

- Displays 20 bays and their live occupancy on the dashboard and kiosk board.
- Registers vehicle plate, driver, arrival time, and allocated bay.
- Calculates duration and the current charge at exit.
- Requires an operator to confirm received cash or verify an M-Pesa/card reference before a paid exit releases the bay.
- Saves configurable time bands, fees, and VAT rate in `data/rates.csv`; changes take effect without rebuilding.
- Writes transaction receipts with payment method/reference, gross amount, and VAT component to `data/transactions.csv`.
- Provides a transaction history with revenue and VAT totals for reconciliation.

Default rates retain the original schedule: free for 30 minutes, KSh 50 through 120 minutes, KSh 100 through 360 minutes, and KSh 500 above that. Management can change the time limits, amounts, and VAT rate in the dashboard.

## Payment and deployment boundary

This prototype records an operator's confirmation and reference after they verify a payment externally. It does not initiate or verify live M-Pesa or card transactions. Cash must be physically received before confirmation. The current HTTP server also has no user authentication; do not expose it to an untrusted network. Production deployment requires authenticated management access, a payment-provider integration/callback, and protected storage.

## API

- `GET /api/status` — bay map, availability, occupancy, revenue, and VAT total.
- `POST /api/arrival` — form fields `plate`, `owner`.
- `POST /api/exit` — form field `plate`; returns a quote without releasing the bay when a fee is due.
- `POST /api/payment` — form fields `plate`, `method`, `reference`, `quotedFee`; confirms payment and opens the barrier in the simulation.
- `GET /api/rates` and `POST /api/rates` — read or update rate bands and VAT.
- `GET /api/search?plate=...` — find an active vehicle.
- `GET /api/transactions` — completed receipt and payment records.
- `GET /` — operator dashboard; `GET /board` — entrance display.

Transaction CSV rows from the earlier seven-column format remain readable. New records include method, reference, receipt number, and VAT amount. See [docs/DATABASE_DESIGN.md](docs/DATABASE_DESIGN.md) for the file schemas.

## Out of scope

Online prebooking, valet operations, third-party loyalty integrations, and automated number-plate blacklisting are not implemented in this phase.
