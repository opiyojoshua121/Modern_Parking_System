#include "ParkingSystem.h"

#include <algorithm>
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
    if (minutes <= 30) return 0.0;
    if (minutes <= 120) return 50.0;
    if (minutes <= 360) return 100.0;
    return 500.0;
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
            getline(ss, plate, ',');
            getline(ss, slotId, ',');
            getline(ss, arrival, ',');
            getline(ss, departure, ',');
            getline(ss, duration, ',');
            getline(ss, fee, ',');
            getline(ss, status, ',');
            try {
                transactions.push_back({trim(plate), stoi(slotId), trim(arrival), trim(departure),
                                         stoll(duration), stod(fee), trim(status)});
            } catch (...) {}
        }
    }
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
    file << "plate,slot,arrival,departure,duration_minutes,fee,payment_status\n";
    for (const auto& t : transactions) {
        file << csvSafe(t.plateNumber) << ',' << t.slotId << ',' << t.arrival << ',' << t.departure << ','
             << t.durationMinutes << ',' << fixed << setprecision(2) << t.fee << ',' << csvSafe(t.paymentStatus) << '\n';
    }
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
    if (plate.empty() || owner.empty()) return {false, "Registration number and driver name are required.", -1, 0, 0.0, "", ""};

    lock_guard<mutex> lock(dataMutex);
    if (findAvailableSlotUnlocked() == -1) return {false, "Parking is full. No vehicle can be admitted.", -1, 0, 0.0, "", ""};
    if (activeVehicles.find(plate) != activeVehicles.end()) return {false, "This vehicle is already inside the parking area.", -1, 0, 0.0, "", ""};

    const int slotId = findAvailableSlotUnlocked();
    const int index = findSlotIndexUnlocked(slotId);
    if (index < 0) return {false, "Unable to allocate a parking slot.", -1, 0, 0.0, "", ""};

    const auto arrival = chrono::system_clock::now();
    slots[index].occupied = true;
    slots[index].plateNumber = plate;
    activeVehicles[plate] = {plate, owner, slotId, arrival};

    saveSlots();
    saveActiveVehicles();

    return {true, "Vehicle admitted successfully.", slotId, 0, 0.0, timePointToString(arrival), ""};
}

OperationResult ParkingSystem::processExit(const string& rawPlate) {
    const string plate = trim(rawPlate);
    if (plate.empty()) return {false, "Registration number is required.", -1, 0, 0.0, "", ""};

    lock_guard<mutex> lock(dataMutex);
    const auto it = activeVehicles.find(plate);
    if (it == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", ""};

    const ActiveVehicle vehicle = it->second;
    const auto exitTime = chrono::system_clock::now();
    const auto seconds = chrono::duration_cast<chrono::seconds>(exitTime - vehicle.arrivalTime).count();
    const long long minutes = max<long long>(0, (seconds + 59) / 60);
    const double fee = calculateFee(minutes);
    const string arrival = timePointToString(vehicle.arrivalTime);
    const string departure = timePointToString(exitTime);

    const int index = findSlotIndexUnlocked(vehicle.slotId);
    if (index >= 0) {
        slots[index].occupied = false;
        slots[index].plateNumber.clear();
    }

    transactions.push_back({vehicle.plateNumber, vehicle.slotId, arrival, departure, minutes, fee,
                            fee == 0.0 ? "FREE" : "PAID"});
    activeVehicles.erase(it);

    saveSlots();
    saveActiveVehicles();
    saveTransactions();

    stringstream message;
    message << "Exit processed. Barrier OPEN. Amount due: KSh " << fixed << setprecision(2) << fee;
    return {true, message.str(), vehicle.slotId, minutes, fee, arrival, departure};
}

OperationResult ParkingSystem::searchVehicle(const string& rawPlate) const {
    const string plate = trim(rawPlate);
    lock_guard<mutex> lock(dataMutex);
    const auto it = activeVehicles.find(plate);
    if (it == activeVehicles.end()) return {false, "Vehicle is not currently in the parking area.", -1, 0, 0.0, "", ""};
    return {true, "Vehicle found.", it->second.slotId, 0, 0.0, timePointToString(it->second.arrivalTime), ""};
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
