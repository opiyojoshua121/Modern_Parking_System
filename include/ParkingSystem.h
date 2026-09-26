#ifndef PARKING_SYSTEM_H
#define PARKING_SYSTEM_H

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ParkingSlot {
    int id{};
    bool occupied{};
    std::string plateNumber;
};

struct ActiveVehicle {
    std::string plateNumber;
    std::string ownerName;
    int slotId{};
    std::chrono::system_clock::time_point arrivalTime;
};

struct ParkingTransaction {
    std::string plateNumber;
    int slotId{};
    std::string arrival;
    std::string departure;
    long long durationMinutes{};
    double fee{};
    std::string paymentStatus;
};

struct OperationResult {
    bool success{false};
    std::string message;
    int slotId{-1};
    long long durationMinutes{0};
    double fee{0.0};
    std::string arrival;
    std::string departure;
};

class ParkingSystem {
private:
    std::vector<ParkingSlot> slots;
    std::unordered_map<std::string, ActiveVehicle> activeVehicles;
    std::vector<ParkingTransaction> transactions;

    const std::string slotsFile = "data/slots.csv";
    const std::string activeFile = "data/active_vehicles.csv";
    const std::string transactionsFile = "data/transactions.csv";

    mutable std::mutex dataMutex;

    void createDefaultSlots();
    void loadData();
    void saveSlots() const;
    void saveActiveVehicles() const;
    void saveTransactions() const;

    static std::string nowString();
    static std::string timePointToString(const std::chrono::system_clock::time_point& timePoint);
    static std::chrono::system_clock::time_point stringToTimePoint(const std::string& value);

    int findAvailableSlotUnlocked() const;
    int findSlotIndexUnlocked(int slotId) const;

public:
    ParkingSystem();

    // Business operations used by both the web interface and tests.
    std::vector<ParkingSlot> getSlots() const;
    std::vector<ParkingTransaction> getTransactions() const;
    std::vector<ActiveVehicle> getActiveVehicles() const;
    OperationResult registerArrival(const std::string& plate, const std::string& owner);
    OperationResult processExit(const std::string& plate);
    OperationResult searchVehicle(const std::string& plate) const;

    int totalSlots() const;
    int availableSlots() const;
    int occupiedSlots() const;
    int activeVehicleCount() const;
    int completedTripCount() const;
    double totalRevenue() const;
};

// MMU fee schedule from the assignment brief.
double calculateFee(long long minutes);

#endif
