# Algorithms — MMU Modern Parking System

## 1. Display available parking slots

```text
FOR each slot in slots
    IF slot.occupied = false
        display AVAILABLE
    ELSE
        display OCCUPIED with registration number
    END IF
END FOR
count available and occupied slots
```

Time complexity: **O(n)**.

## 2. Vehicle arrival and automatic allocation

```text
RECEIVE registration number and driver name
IF either field is empty: reject
IF no available slot exists: reject as FULL
IF registration already exists in activeVehicles: reject duplicate
SCAN slots from first to last
SELECT first slot where occupied = false
MARK selected slot occupied and store plate
CREATE ActiveVehicle with current timestamp
INSERT into activeVehicles using plate as key
SAVE slot and active-vehicle files
RETURN allocated slot
```

Time complexity: **O(n)** worst case for slot search; duplicate lookup is **O(1) average**.

## 3. Search active vehicle

```text
RECEIVE registration number
LOOK UP registration in activeVehicles
IF found: return owner, slot and arrival time
ELSE: report not currently inside
```

Time complexity: **O(1) average**.

## 4. Vehicle exit and fee calculation

```text
RECEIVE registration number
LOOK UP vehicle in activeVehicles
IF not found: reject exit
exitTime = current time
minutes = ceiling((exitTime - arrivalTime) / 60 seconds)
fee = apply the current persisted rate bands
IF fee = 0: record a FREE receipt and release the slot
ELSE: return the fee quote; keep vehicle and slot active
WAIT for operator-confirmed payment and required provider reference
IF confirmed amount differs from current quote: reject and require recalculation
CREATE transaction with method, reference, receipt and VAT component
FREE the vehicle's slot and REMOVE it from activeVehicles
SAVE database files
REPORT payment result and barrier state
```

## 5. Transaction history and revenue

Iterate through completed transactions to display receipt, payment method/reference, gross amount and VAT, and sum the `fee` and `vat_amount` fields. Operations are **O(t)** where `t` is the number of completed transactions.

## 6. Web request handling

```text
START TCP listening socket on port 8080
WHILE server is running
    ACCEPT browser connection
    START a C++ thread
    PARSE HTTP method, path and body
    ROUTE request to parking module or static web file
    RETURN HTTP response
    CLOSE connection
END WHILE
```

