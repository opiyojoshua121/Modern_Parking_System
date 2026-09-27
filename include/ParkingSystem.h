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
    std::string paymentMethod;
    std::string paymentReference;
    std::string receiptNumber;
    double vatAmount{};
};

struct ParkingRates {
    long long freeMinutes{30};
    long long firstBandMinutes{120};
    long long secondBandMinutes{360};
    double firstBandFee{50.0};
    double secondBandFee{100.0};
    double overSecondBandFee{500.0};
    double vatPercent{16.0};
};

struct OperationResult {
    bool success{false};
    std::string message;
    int slotId{-1};
    long long durationMinutes{0};
    double fee{0.0};
    std::string arrival;
    std::string departure;
    std::string receiptNumber;
    double vatAmount{0.0};
};

class ParkingSystem {
private:
    std::vector<ParkingSlot> slots;
    std::unordered_map<std::string, ActiveVehicle> activeVehicles;
    std::vector<ParkingTransaction> transactions;
    ParkingRates rates;

    const std::string slotsFile = "data/slots.csv";
    const std::string activeFile = "data/active_vehicles.csv";
    const std::string transactionsFile = "data/transactions.csv";
    const std::string ratesFile = "data/rates.csv";

    mutable std::mutex dataMutex;

    void createDefaultSlots();
    void loadData();
    void saveSlots() const;
    void saveActiveVehicles() const;
    void saveTransactions() const;
    void loadRates();
    void saveRates() const;
    OperationResult completeExitUnlocked(const std::string& plate, const std::string& paymentMethod,
                                         const std::string& paymentReference, double expectedFee);

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
    OperationResult confirmPayment(const std::string& plate, const std::string& paymentMethod,
                                   const std::string& paymentReference, double expectedFee);
    OperationResult searchVehicle(const std::string& plate) const;
    ParkingRates getRates() const;
    bool updateRates(const ParkingRates& newRates, std::string& error);

    int totalSlots() const;
    int availableSlots() const;
    int occupiedSlots() const;
    int activeVehicleCount() const;
    int completedTripCount() const;
    double totalRevenue() const;
    double totalVat() const;
};

// MMU fee schedule from the assignment brief.
double calculateFee(long long minutes);
double calculateFee(long long minutes, const ParkingRates& rates);

#endif
