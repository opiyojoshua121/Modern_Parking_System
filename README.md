# MMU Modern Parking System 
 
**Project type:** Web-based parking management system

## 1. Project purpose

This project implements the parking-system requirements in the supplied MMU task brief. The system allows drivers/attendants to see parking-slot availability before entry, register arriving vehicles, automatically allocate a slot, calculate parking time at exit, calculate the required fee, record payment, simulate opening the barrier, and keep persistent transaction records.

The backend is written in C++17. A small cross-platform HTTP server is included, so no Node.js, PHP, Python web framework, or external C++ web framework is required. The browser interface is HTML/CSS/JavaScript and communicates with the C++ server through HTTP endpoints.

## 2. Fee rules implemented

| Parking duration | Fee |
|---|---:|
| Up to 30 minutes | FREE |
| Over 30 minutes up to 2 hours | KSh 50 |
| Over 2 hours up to 6 hours | KSh 100 |
| Over 6 hours | KSh 500 |

## 3. Main modules

1. **Parking Slot Management** — displays 20 slots and their status.
2. **Vehicle Arrival / Admission** — validates input, prevents duplicate active registrations and allocates the first available slot.
3. **Vehicle Search / Monitoring** — finds an active vehicle by registration number.
4. **Vehicle Exit & Fee Calculation** — calculates elapsed time and applies the MMU fee rules.
5. **Payment & Barrier Control** — records a completed transaction and reports that the barrier is OPEN.
6. **Transaction History & Reporting** — displays completed parking transactions and total revenue.
7. **Dynamic Persistence** — stores slot, active-vehicle and transaction data in CSV database files.
8. **Web/API Layer** — serves the dashboard and JSON-style HTTP API endpoints.

## 4. Data structures used

- `std::vector<ParkingSlot>`: stores the fixed parking-slot collection and supports ordered visual display.
- `std::unordered_map<string, ActiveVehicle>`: maps registration number to an active vehicle, giving average O(1) lookup for search, duplicate detection and exit processing.
- `std::vector<ParkingTransaction>`: stores completed transactions in insertion order.
- `std::mutex`: protects shared in-memory data because each HTTP request is handled by a separate C++ thread.

## 5. Dynamic database design

The project uses a simple persistent file-based database so it can run without installing MySQL or another DBMS.

### `data/slots.csv`

| Field | Type | Description |
|---|---|---|
| id | INT | Unique slot number |
| occupied | BOOLEAN | 1 if occupied, otherwise 0 |
| plate | VARCHAR | Registration of occupying vehicle |

### `data/active_vehicles.csv`

| Field | Type | Description |
|---|---|---|
| plate | VARCHAR | Vehicle registration; logical key |
| owner | VARCHAR | Driver/owner name |
| slot | INT | Allocated slot |
| arrival | DATETIME | Arrival date and time |

### `data/transactions.csv`

| Field | Type | Description |
|---|---|---|
| plate | VARCHAR | Vehicle registration |
| slot | INT | Slot used |
| arrival | DATETIME | Entry time |
| departure | DATETIME | Exit time |
| duration_minutes | BIGINT | Total parking time |
| fee | DECIMAL | Amount charged in KSh |
| payment_status | VARCHAR | FREE or PAID |

## 6. Web API endpoints

- `GET /api/status` — slot map and system summary.
- `POST /api/arrival` — registers an arrival using `plate` and `owner` form fields.
- `POST /api/exit` — processes exit using the `plate` form field.
- `GET /api/search?plate=...` — searches active vehicles.
- `GET /api/transactions` — returns completed transactions.
- `GET /` — loads the web dashboard.

## 7. Project structure

```text
MMU-Modern-Parking-System/
├── .vscode/
│   ├── launch.json
│   └── tasks.json
├── data/
│   ├── active_vehicles.csv
│   ├── slots.csv
│   └── transactions.csv
├── docs/
│   ├── ALGORITHMS.md
│   └── DATABASE_DESIGN.md
├── include/
│   ├── ParkingSystem.h
│   └── WebServer.h
├── src/
│   ├── main.cpp
│   ├── ParkingSystem.cpp
│   └── WebServer.cpp
├── web/
│   ├── index.html
│   ├── style.css
│   └── app.js
├── build.bat
├── run.bat
├── run.sh
├── .gitignore
├── Parking_System_Assignment.docx
└── README.me

## 8. How to demonstrate the system

1. Open the dashboard and show the 20 available slots.
2. Enter a registration number such as `KDA 123A` and a driver name.
3. Click **Admit Vehicle**. Slot 01 becomes occupied.
4. Use **Find Active Vehicle** to locate the vehicle and show its allocated slot.
5. Use **Vehicle Exit** with the same registration. The system calculates the actual elapsed time, applies the fee rule, records the transaction and frees the slot.
6. Refresh the page and show transaction history and revenue.

For a quick classroom demonstration, a vehicle exited within 30 minutes produces a FREE transaction. To demonstrate KSh 50, KSh 100 or KSh 500, the vehicle 
