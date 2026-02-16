#pragma once
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Gameplay {

// =========================================================================
// Transaction Tracking
// =========================================================================

enum class TransactionType : uint8_t {
  TicketSales,
  FoodSales,
  MerchandiseSales,
  ConstructionCost,
  MaintenanceCost,
  StaffSalary,
  ResearchCost,
  BreedingCost,
  FeedCost,
  LoanPayment,
  SpecialEvent,
  Insurance
};

struct Transaction {
  TransactionType type;
  float amount;    // Positive = income, negative = expense
  float timestamp; // Game time
  std::string description;
};

// =========================================================================
// Economy System
// =========================================================================

class EconomySystem {
  float balance = 500000.0f; // Starting capital
  float totalIncome = 0;
  float totalExpenses = 0;
  float ticketPrice = 50.0f;
  float taxRate = 0.15f;
  std::vector<Transaction> history;
  std::vector<Transaction> recentTransactions;

  // Revenue per tick (accumulated from various sources)
  float tickRevenue = 0;
  float tickExpenses = 0;

  // Loan
  float loanBalance = 0;
  float loanInterestRate = 0.05f; // 5% per game day

  // Insurance
  float insurancePremium = 1000.0f;
  bool hasInsurance = false;

  float gameTime = 0;

public:
  void Initialize(float startingCapital = 500000.0f) {
    balance = startingCapital;
    totalIncome = 0;
    totalExpenses = 0;
    history.clear();
    std::cout << "[Economy] Initialized with $" << startingCapital << std::endl;
  }

  // --- Core transactions ---
  bool Spend(float amount, TransactionType type, const std::string &desc = "") {
    if (amount <= 0)
      return false;
    if (balance < amount) {
      std::cout << "[Economy] Insufficient funds! Need $" << amount
                << " but have $" << balance << std::endl;
      return false;
    }
    balance -= amount;
    totalExpenses += amount;
    tickExpenses += amount;
    RecordTransaction(type, -amount, desc);
    return true;
  }

  void Earn(float amount, TransactionType type, const std::string &desc = "") {
    if (amount <= 0)
      return;
    balance += amount;
    totalIncome += amount;
    tickRevenue += amount;
    RecordTransaction(type, amount, desc);
  }

  // --- Visitor Economics ---
  void ProcessVisitors(uint32_t visitorCount, float avgSatisfaction) {
    float ticketRevenue = visitorCount * ticketPrice;
    float satisfactionMultiplier = 0.5f + avgSatisfaction; // [0.5..1.5]
    float adjustedRevenue = ticketRevenue * satisfactionMultiplier;
    Earn(adjustedRevenue, TransactionType::TicketSales,
         std::to_string(visitorCount) + " visitors");

    // Food and merchandise spending
    float spendPerVisitor = (15.0f + avgSatisfaction * 30.0f);
    float foodRev = visitorCount * spendPerVisitor * 0.4f;
    float shopRev = visitorCount * spendPerVisitor * 0.2f;
    Earn(foodRev, TransactionType::FoodSales);
    Earn(shopRev, TransactionType::MerchandiseSales);
  }

  // --- Recurring costs ---
  void ProcessMaintenanceCosts(float maintenanceCost) {
    Spend(maintenanceCost, TransactionType::MaintenanceCost,
          "Building maintenance");
  }

  void ProcessStaffSalaries(uint32_t staffCount) {
    float salary = staffCount * 500.0f; // $500 per staff per tick
    Spend(salary, TransactionType::StaffSalary,
          std::to_string(staffCount) + " staff");
  }

  void ProcessDinosaurFeeding(uint32_t dinoCount) {
    float feedCost = dinoCount * 100.0f;
    Spend(feedCost, TransactionType::FeedCost,
          std::to_string(dinoCount) + " dinosaurs");
  }

  // --- Loans ---
  bool TakeLoan(float amount) {
    if (loanBalance > 0) {
      std::cout << "[Economy] Already have outstanding loan of $" << loanBalance
                << std::endl;
      return false;
    }
    loanBalance = amount;
    balance += amount;
    std::cout << "[Economy] Loan taken: $" << amount << " at "
              << (loanInterestRate * 100) << "% interest" << std::endl;
    return true;
  }

  void ProcessLoanPayment() {
    if (loanBalance <= 0)
      return;
    float interest = loanBalance * loanInterestRate;
    float payment =
        std::min(loanBalance + interest, std::max(1000.0f, loanBalance * 0.1f));
    if (balance >= payment) {
      balance -= payment;
      loanBalance -= (payment - interest);
      if (loanBalance < 1.0f)
        loanBalance = 0;
      RecordTransaction(TransactionType::LoanPayment, -payment,
                        "Loan repayment");
    }
  }

  // --- Insurance ---
  void PurchaseInsurance() {
    if (hasInsurance)
      return;
    if (Spend(insurancePremium * 10, TransactionType::Insurance,
              "Annual premium")) {
      hasInsurance = true;
      std::cout << "[Economy] Insurance purchased" << std::endl;
    }
  }

  // --- Taxes ---
  float ProcessTaxes() {
    if (tickRevenue <= tickExpenses)
      return 0;
    float profit = tickRevenue - tickExpenses;
    float tax = profit * taxRate;
    balance -= tax;
    return tax;
  }

  // --- Tick processing ---
  void Update(float dt) {
    gameTime += dt;
    tickRevenue = 0;
    tickExpenses = 0;
    recentTransactions.clear();
  }

  // --- Queries ---
  float GetBalance() const { return balance; }
  float GetTotalIncome() const { return totalIncome; }
  float GetTotalExpenses() const { return totalExpenses; }
  float GetProfit() const { return totalIncome - totalExpenses; }
  float GetTicketPrice() const { return ticketPrice; }
  float GetLoanBalance() const { return loanBalance; }
  bool HasInsurance() const { return hasInsurance; }

  void SetTicketPrice(float price) {
    ticketPrice = std::clamp(price, 10.0f, 500.0f);
    std::cout << "[Economy] Ticket price set to $" << ticketPrice << std::endl;
  }

  bool CanAfford(float amount) const { return balance >= amount; }

  void PrintFinancialReport() const {
    std::cout << "\n=== FINANCIAL REPORT ===" << std::endl;
    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  Balance:    $" << balance << std::endl;
    std::cout << "  Income:     $" << totalIncome << std::endl;
    std::cout << "  Expenses:   $" << totalExpenses << std::endl;
    std::cout << "  Profit:     $" << GetProfit()
              << (GetProfit() >= 0 ? " ✅" : " ⚠️") << std::endl;
    std::cout << "  Ticket:     $" << ticketPrice << std::endl;
    if (loanBalance > 0)
      std::cout << "  Loan:       $" << loanBalance << " outstanding"
                << std::endl;
    if (hasInsurance)
      std::cout << "  Insurance:  Active" << std::endl;
    std::cout << "  Tax Rate:   " << (taxRate * 100) << "%" << std::endl;
    std::cout << "  Transactions: " << history.size() << " total" << std::endl;
  }

private:
  void RecordTransaction(TransactionType type, float amount,
                         const std::string &desc = "") {
    Transaction t;
    t.type = type;
    t.amount = amount;
    t.timestamp = gameTime;
    t.description = desc;
    history.push_back(t);
    recentTransactions.push_back(t);

    // Cap history
    if (history.size() > 10000) {
      history.erase(history.begin(), history.begin() + 5000);
    }
  }
};

} // namespace Gameplay
} // namespace Mesozoic
