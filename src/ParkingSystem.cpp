#include "ParkingSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {
string trim(const string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

string csvSafe(string value) {
    replace(value.begin(), value.end(), ',', ' ');
    replace(value.begin(), value.end(), '\n', ' ');
    replace(value.begin(), value.end(), '\r', ' ');
    return value;
}
}

double calculateFee(long long minutes) {
    return calculateFee(minutes, ParkingRates{});
}

double calculateFee(long long minutes, const ParkingRates& schedule) {
    if (minutes <= schedule.freeMinutes) return 0.0;
    if (minutes <= schedule.firstBandMinutes) return schedule.firstBandFee;
    if (minutes <= schedule.secondBandMinutes) return schedule.secondBandFee;
    return schedule.overSecondBandFee;
}

ParkingSystem::ParkingSystem() {
    filesystem::create_directories("data");
    loadData();
}

void ParkingSystem::createDefaultSlots() {
    slots.clear();
    for (int i = 1; i <= 20; ++i) {
        slots.push_back({i, false, ""});
    }
    saveSlots();
}

string ParkingSystem::nowString() {
    return timePointToString(chrono::system_clock::now());
}

string ParkingSystem::timePointToString(const chrono::system_clock::time_point& timePoint) {
    time_t timeValue = chrono::system_clock::to_time_t(timePoint);
    tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif
    stringstream ss;
    ss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

chrono::system_clock::time_point ParkingSystem::stringToTimePoint(const string& value) {
    tm localTime{};
    stringstream ss(value);
    ss >> get_time(&localTime, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return chrono::system_clock::now();
    time_t timeValue = mktime(&localTime);
    return chrono::system_clock::from_time_t(timeValue);
}

int ParkingSystem::findAvailableSlotUnlocked() const {
    for (const auto& slot : slots) {
        if (!slot.occupied) return slot.id;
    }
    return -1;
}

int ParkingSystem::findSlotIndexUnlocked(int slotId) const {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].id == slotId) return static_cast<int>(i);
    }
    return -1;
}

void ParkingSystem::loadData() {
    lock_guard<mutex> lock(dataMutex);

    ifstream slotFile(slotsFile);
    if (!slotFile) {
        createDefaultSlots();
    } else {
        slots.clear();
        string line;
        getline(slotFile, line);
        while (getline(slotFile, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string id, occupied, plate;
            getline(ss, id, ',');
            getline(ss, occupied, ',');
            getline(ss, plate, ',');
            try {
                slots.push_back({stoi(id), occupied == "1", trim(plate)});
            } catch (...) {
                // Ignore malformed rows instead of stopping the whole server.
            }
        }
        if (slots.empty()) {
            for (int i = 1; i <= 20; ++i) slots.push_back({i, false, ""});
            saveSlots();
        }
    }

    activeVehicles.clear();
    ifstream activeFileStream(activeFile);
    if (activeFileStream) {
        string line;
        getline(activeFileStream, line);
        while (getline(activeFileStream, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string plate, owner, slotId, arrival;
            getline(ss, plate, ',');
            getline(ss, owner, ',');
            getline(ss, slotId, ',');
            getline(ss, arrival, ',');
            try {
                activeVehicles[trim(plate)] = {trim(plate), trim(owner), stoi(slotId), stringToTimePoint(trim(arrival))};
            } catch (...) {}
        }
    }

    transactions.clear();
    ifstream transactionFile(transactionsFile);
    if (transactionFile) {
        string line;
        getline(transactionFile, line);
        while (getline(transactionFile, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string plate, slotId, arrival, departure, duration, fee, status;
            string method, reference, receipt, vat;
            getline(ss, plate, ',');
            getline(ss, slotId, ',');
            getline(ss, arrival, ',');
            getline(ss, departure, ',');
            getline(ss, duration, ',');
            getline(ss, fee, ',');
            getline(ss, status, ',');
            getline(ss, method, ',');
            getline(ss, reference, ',');
            getline(ss, receipt, ',');
            getline(ss, vat, ',');
            try {
                transactions.push_back({trim(plate), stoi(slotId), trim(arrival), trim(departure),
                                         stoll(duration), stod(fee), trim(status), trim(method),
                                         trim(reference), trim(receipt), vat.empty() ? 0.0 : stod(vat)});
            } catch (...) {}
        }
    }

    loadRates();
}

void ParkingSystem::saveSlots() const {
    ofstream file(slotsFile, ios::trunc);
    file << "id,occupied,plate\n";
    for (const auto& slot : slots) {
        file << slot.id << ',' << (slot.occupied ? 1 : 0) << ',' << csvSafe(slot.plateNumber) << '\n';
    }
}

void ParkingSystem::saveActiveVehicles() const {
    ofstream file(activeFile, ios::trunc);
    file << "plate,owner,slot,arrival\n";
    for (const auto& [plate, vehicle] : activeVehicles) {
        file << csvSafe(vehicle.plateNumber) << ',' << csvSafe(vehicle.ownerName) << ','
             << vehicle.slotId << ',' << timePointToString(vehicle.arrivalTime) << '\n';
    }
}

void ParkingSystem::saveTransactions() const {
    ofstream file(transactionsFile, ios::trunc);
    file << "plate,slot,arrival,departure,duration_minutes,fee,payment_status,payment_method,"
            "payment_reference,receipt_number,vat_amount\n";
    for (const auto& t : transactions) {
        file << csvSafe(t.plateNumber) << ',' << t.slotId << ',' << t.arrival << ',' << t.departure << ','
             << t.durationMinutes << ',' << fixed << setprecision(2) << t.fee << ',' << csvSafe(t.paymentStatus)
             << ',' << csvSafe(t.paymentMethod) << ',' << csvSafe(t.paymentReference) << ','
             << csvSafe(t.receiptNumber) << ',' << fixed << setprecision(2) << t.vatAmount << '\n';
    }
}

void ParkingSystem::loadRates() {
    ifstream file(ratesFile);
    if (!file) {
        saveRates();
        return;
    }
    string header;
    string line;
    getline(file, header);
    if (!getline(file, line)) return;
    stringstream row(line);
    string freeMinutes, firstMinutes, secondMinutes, firstFee, secondFee, overFee, vat;
    getline(row, freeMinutes, ',');
    getline(row, firstMinutes, ',');
    getline(row, secondMinutes, ',');
    getline(row, firstFee, ',');
    getline(row, secondFee, ',');
    getline(row, overFee, ',');
    getline(row, vat, ',');
    try {
        ParkingRates loaded{stoll(freeMinutes), stoll(firstMinutes), stoll(secondMinutes),
                            stod(firstFee), stod(secondFee), stod(overFee), stod(vat)};
        if (loaded.freeMinutes >= 0 && loaded.firstBandMinutes > loaded.freeMinutes &&
            loaded.secondBandMinutes > loaded.firstBandMinutes && loaded.firstBandFee >= 0 &&
            loaded.secondBandFee >= 0 && loaded.overSecondBandFee >= 0 &&
            loaded.vatPercent >= 0 && loaded.vatPercent <= 100) {
            rates = loaded;
        }
    } catch (...) {}
}

void ParkingSystem::saveRates() const {
    ofstream file(ratesFile, ios::trunc);
    file << "free_minutes,first_band_minutes,second_band_minutes,first_band_fee,second_band_fee,"
            "over_second_band_fee,vat_percent\n"
         << rates.freeMinutes << ',' << rates.firstBandMinutes << ',' << rates.secondBandMinutes << ','
         << fixed << setprecision(2) << rates.firstBandFee << ',' << rates.secondBandFee << ','
         << rates.overSecondBandFee << ',' << rates.vatPercent << '\n';
}

vector<ParkingSlot> ParkingSystem::getSlots() const {
    lock_guard<mutex> lock(dataMutex);
    return slots;
}

vector<ParkingTransaction> ParkingSystem::getTransactions() const {
    lock_guard<mutex> lock(dataMutex);
    return transactions;
}

vector<ActiveVehicle> ParkingSystem::getActiveVehicles() const {
    lock_guard<mutex> lock(dataMutex);
    vector<ActiveVehicle> result;
    result.reserve(activeVehicles.size());
    for (const auto& [_, vehicle] : activeVehicles) result.push_back(vehicle);
    sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.slotId < b.slotId; });
    return result;
}

OperationResult ParkingSystem::registerArrival(const string& rawPlate, const string& rawOwner) {
    const string plate = trim(rawPlate);
    const string owner = trim(rawOwner);
    if (plate.empty() || owner.empty()) return {false, "Registration number and driver name are required.", -1, 0, 0.0, "", "", "", 0.0};

    lock_guard<mutex> lock(dataMutex);
    if (findAvailableSlotUnlocked() == -1) return {false, "Parking is full. No vehicle can be admitted.", -1, 0, 0.0, "", "", "", 0.0};
    if (activeVehicles.find(plate) != activeVehicles.end()) return {false, "This vehicle is already inside the parking area.", -1, 0, 0.0, "", "", "", 0.0};

    const int slotId = findAvailableSlotUnlocked();
    const int index = findSlotIndexUnlocked(slotId);
    if (index < 0) return {false, "Unable to allocate a parking slot.", -1, 0, 0.0, "", "", "", 0.0};

    const auto arrival = chrono::system_clock::now();
    slots[index].occupied = true;
    slots[index].plateNumber = plate;
    activeVehicles[plate] = {plate, owner, slotId, arrival};

    saveSlots();
    saveActiveVehicles();

    return {true, "Vehicle admitted successfully.", slotId, 0, 0.0, timePointToString(arrival), "", "", 0.0};
}

OperationResult ParkingSystem::processExit(const string& rawPlate) {
    const string plate = trim(rawPlate);
    if (plate.empty()) return {false, "Registration number is required.", -1, 0, 0.0, "", "", "", 0.0};

    lock_guard<mutex> lock(dataMutex);
    const auto it = activeVehicles.find(plate);
    if (it == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", "", "", 0.0};

    const ActiveVehicle vehicle = it->second;
    const auto exitTime = chrono::system_clock::now();
    const auto elapsed = exitTime - vehicle.arrivalTime;
    long long minutes = chrono::duration_cast<chrono::minutes>(elapsed).count();
    if (elapsed > chrono::minutes(minutes)) ++minutes;
    minutes = max<long long>(0, minutes);
    const double fee = calculateFee(minutes, rates);
    const string arrival = timePointToString(vehicle.arrivalTime);
    const string departure = timePointToString(exitTime);

    if (fee == 0.0) return completeExitUnlocked(plate, "FREE", "", 0.0);

    stringstream message;
    message << "Amount due: KSh " << fixed << setprecision(2) << fee
            << ". Confirm the received payment to open the barrier.";
    return {true, message.str(), vehicle.slotId, minutes, fee, arrival, departure, "", 0.0};
}

OperationResult ParkingSystem::completeExitUnlocked(const string& plate, const string& paymentMethod,
                                                       const string& paymentReference, double expectedFee) {
    const auto it = activeVehicles.find(plate);
    if (it == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", "", "", 0.0};
    const ActiveVehicle vehicle = it->second;
    const auto exitTime = chrono::system_clock::now();
    const auto elapsed = exitTime - vehicle.arrivalTime;
    long long minutes = chrono::duration_cast<chrono::minutes>(elapsed).count();
    if (elapsed > chrono::minutes(minutes)) ++minutes;
    minutes = max<long long>(0, minutes);
    const double fee = calculateFee(minutes, rates);
        if (fee != expectedFee) {
        stringstream message;
        message << "The fee changed to KSh " << fixed << setprecision(2) << fee
            << ". Recalculate and confirm the updated amount.";
        return {false, message.str(), vehicle.slotId, minutes, fee, timePointToString(vehicle.arrivalTime),
            timePointToString(exitTime), "", 0.0};
        }
    const string arrival = timePointToString(vehicle.arrivalTime);
    const string departure = timePointToString(exitTime);
    const double vat = rates.vatPercent > 0.0 ? fee * rates.vatPercent / (100.0 + rates.vatPercent) : 0.0;
    stringstream receipt;
    receipt << "MMU-" << setfill('0') << setw(8) << transactions.size() + 1;

    const int index = findSlotIndexUnlocked(vehicle.slotId);
    if (index >= 0) {
        slots[index].occupied = false;
        slots[index].plateNumber.clear();
    }

    transactions.push_back({vehicle.plateNumber, vehicle.slotId, arrival, departure, minutes, fee,
                            fee == 0.0 ? "FREE" : "PAID", paymentMethod, paymentReference,
                            receipt.str(), vat});
    activeVehicles.erase(it);

    saveSlots();
    saveActiveVehicles();
    saveTransactions();

    stringstream message;
    message << (fee == 0.0 ? "No payment due." : "Payment confirmed.")
            << " Receipt " << receipt.str() << ". Barrier OPEN.";
    return {true, message.str(), vehicle.slotId, minutes, fee, arrival, departure, receipt.str(), vat};
}

OperationResult ParkingSystem::confirmPayment(const string& rawPlate, const string& rawMethod,
                                                const string& rawReference, double expectedFee) {
    const string plate = trim(rawPlate);
    string method = trim(rawMethod);
    string reference = trim(rawReference);
    transform(method.begin(), method.end(), method.begin(), [](unsigned char c) { return static_cast<char>(toupper(c)); });
    if (plate.empty()) return {false, "Registration number is required.", -1, 0, 0.0, "", "", "", 0.0};
    if (method != "CASH" && method != "MPESA" && method != "CARD") {
        return {false, "Choose cash, M-Pesa, or card.", -1, 0, 0.0, "", "", "", 0.0};
    }
    if (method != "CASH" && reference.empty()) {
        return {false, "Enter the confirmed M-Pesa or card reference.", -1, 0, 0.0, "", "", "", 0.0};
    }

    lock_guard<mutex> lock(dataMutex);
    const auto vehicle = activeVehicles.find(plate);
    if (vehicle == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", "", "", 0.0};
    if (!isfinite(expectedFee) || expectedFee <= 0.0) return {false, "The quoted fee is invalid. Recalculate the exit.", -1, 0, 0.0, "", "", "", 0.0};
    if (!reference.empty() && any_of(transactions.begin(), transactions.end(), [&](const auto& transaction) {
            return transaction.paymentReference == reference;
        })) {
        return {false, "This payment reference has already been recorded.", -1, 0, 0.0, "", "", "", 0.0};
    }
    return completeExitUnlocked(plate, method, reference, expectedFee);
}

OperationResult ParkingSystem::searchVehicle(const string& rawPlate) const {
    const string plate = trim(rawPlate);
    lock_guard<mutex> lock(dataMutex);
    const auto it = activeVehicles.find(plate);
    if (it == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", "", "", 0.0};
    return {true, "Vehicle found.", it->second.slotId, 0, 0.0, timePointToString(it->second.arrivalTime), "", "", 0.0};
}

ParkingRates ParkingSystem::getRates() const {
    lock_guard<mutex> lock(dataMutex);
    return rates;
}

bool ParkingSystem::updateRates(const ParkingRates& newRates, string& error) {
    if (newRates.freeMinutes < 0 || newRates.firstBandMinutes <= newRates.freeMinutes ||
        newRates.secondBandMinutes <= newRates.firstBandMinutes) {
        error = "Time bands must increase: free, first band, then second band.";
        return false;
    }
    if (!isfinite(newRates.firstBandFee) || !isfinite(newRates.secondBandFee) ||
        !isfinite(newRates.overSecondBandFee) || !isfinite(newRates.vatPercent) ||
        newRates.firstBandFee < 0 || newRates.secondBandFee < 0 || newRates.overSecondBandFee < 0 ||
        newRates.vatPercent < 0 || newRates.vatPercent > 100) {
        error = "Rates cannot be negative and VAT must be between 0 and 100 percent.";
        return false;
    }
    lock_guard<mutex> lock(dataMutex);
    rates = newRates;
    saveRates();
    error.clear();
    return true;
}

int ParkingSystem::totalSlots() const {
    lock_guard<mutex> lock(dataMutex);
    return static_cast<int>(slots.size());
}

int ParkingSystem::availableSlots() const {
    lock_guard<mutex> lock(dataMutex);
    return static_cast<int>(count_if(slots.begin(), slots.end(), [](const auto& s) { return !s.occupied; }));
}

int ParkingSystem::occupiedSlots() const {
    return totalSlots() - availableSlots();
}

int ParkingSystem::activeVehicleCount() const {
    lock_guard<mutex> lock(dataMutex);
    return static_cast<int>(activeVehicles.size());
}

int ParkingSystem::completedTripCount() const {
    lock_guard<mutex> lock(dataMutex);
    return static_cast<int>(transactions.size());
}

double ParkingSystem::totalRevenue() const {
    lock_guard<mutex> lock(dataMutex);
    double revenue = 0.0;
    for (const auto& t : transactions) revenue += t.fee;
    return revenue;
}

double ParkingSystem::totalVat() const {
    lock_guard<mutex> lock(dataMutex);
    double vat = 0.0;
    for (const auto& transaction : transactions) vat += transaction.vatAmount;
    return vat;
}
