/*
 * ===========================================================================
 *  PERSONAL EXPENSE TRACKER  (OOPS in C++ - PBL / Mini Project)
 * ===========================================================================
 *  OOP concepts used:
 *    Abstraction ........ abstract base class Transaction
 *    Inheritance ........ Expense and Income derive from Transaction
 *    Polymorphism ....... virtual type(), signedAmount(), extra(), display()
 *    Encapsulation ...... private/protected data + getters
 *    Operator overload .. Date: <, ==, <<   Summary: +
 *    Templates .......... Repository<T> (smart-pointer based container)
 *    Exceptions ......... ExpenseException hierarchy
 *    Smart pointers ..... unique_ptr owns every Transaction
 *    STL ................ vector, map, sort + lambda, algorithm
 *    File handling ...... transactions.txt, budgets.txt, CSV export
 *
 *  UNIQUE FEATURE  ->  "BUDGET GUARDIAN"
 *    1. Monthly budget per category with LIVE alerts (80% warning, 100% over).
 *    2. End-of-month FORECAST per category from the daily spending rate.
 *    3. "Safe to spend per day" for the rest of the month.
 *    4. Financial HEALTH SCORE (0-100) with a grade.
 * ===========================================================================
 */

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// ---------------------------------------------------------------- exceptions
class ExpenseException : public runtime_error {
public:
    explicit ExpenseException(const string& m) : runtime_error(m) {}
};
class InvalidAmountException : public ExpenseException {
public:
    explicit InvalidAmountException(const string& m) : ExpenseException(m) {}
};
class InvalidDateException : public ExpenseException {
public:
    explicit InvalidDateException(const string& m) : ExpenseException(m) {}
};
class NotFoundException : public ExpenseException {
public:
    explicit NotFoundException(const string& m) : ExpenseException(m + " not found.") {}
};

// ------------------------------------------------------------------- helpers
vector<string> split(const string& line, char sep) {
    vector<string> out;
    string part;
    stringstream ss(line);
    while (getline(ss, part, sep)) out.push_back(part);
    return out;
}
string lower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}
string money(double v) {
    ostringstream os;
    os << "Rs " << fixed << setprecision(2) << v;
    return os.str();
}

// ===================================================================== Date
class Date {
    int y, m, d;

public:
    Date(int y = 2026, int m = 1, int d = 1) : y(y), m(m), d(d) {}

    static bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
    static int daysInMonth(int y, int m) {
        static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return (m == 2 && isLeap(y)) ? 29 : dm[m - 1];
    }
    static Date parse(const string& s) {            // expects YYYY-MM-DD
        int yy, mm, dd;
        char a, b;
        stringstream ss(s);
        if (!(ss >> yy >> a >> mm >> b >> dd) || a != '-' || b != '-' || !ss.eof())
            throw InvalidDateException("Date must be in YYYY-MM-DD format.");
        if (yy < 2000 || yy > 2100) throw InvalidDateException("Year must be 2000-2100.");
        if (mm < 1 || mm > 12) throw InvalidDateException("Month must be 1-12.");
        if (dd < 1 || dd > daysInMonth(yy, mm))
            throw InvalidDateException("Day is not valid for that month.");
        return Date(yy, mm, dd);
    }
    static Date today() {
        time_t t = time(nullptr);
        tm* lt = localtime(&t);
        return Date(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    }

    int year() const { return y; }
    int month() const { return m; }
    int day() const { return d; }
    bool sameMonth(int yy, int mm) const { return y == yy && m == mm; }

    string str() const {
        ostringstream os;
        os << setfill('0') << setw(4) << y << "-" << setw(2) << m << "-" << setw(2) << d;
        return os.str();
    }
    bool operator<(const Date& o) const {
        if (y != o.y) return y < o.y;
        if (m != o.m) return m < o.m;
        return d < o.d;
    }
    bool operator==(const Date& o) const { return y == o.y && m == o.m && d == o.d; }
    bool operator<=(const Date& o) const { return *this < o || *this == o; }
    friend ostream& operator<<(ostream& os, const Date& dt) { return os << dt.str(); }
};

// =========================================================== class Transaction
class Transaction {
protected:
    int id;
    Date date;
    double amount;
    string category;
    string note;

public:
    Transaction(int id, const Date& date, double amount, const string& cat, const string& note)
        : id(id), date(date), amount(amount), category(cat), note(note) {
        if (amount <= 0) throw InvalidAmountException("Amount must be greater than zero.");
        if (cat.empty()) throw ExpenseException("Category cannot be empty.");
    }
    virtual ~Transaction() {}

    virtual string type() const = 0;                // "EXPENSE" / "INCOME"
    virtual double signedAmount() const = 0;        // - for expense, + for income
    virtual string extra() const = 0;               // payment mode / source
    virtual void display() const = 0;

    string serialize() const {
        return type() + "|" + to_string(id) + "|" + date.str() + "|" + to_string(amount) +
               "|" + category + "|" + note + "|" + extra();
    }

    int getId() const { return id; }
    const Date& getDate() const { return date; }
    double getAmount() const { return amount; }
    const string& getCategory() const { return category; }
    const string& getNote() const { return note; }

    static unique_ptr<Transaction> fromLine(const string& line);   // defined below
};

class Expense : public Transaction {
    string paymentMode;

public:
    Expense(int id, const Date& d, double amt, const string& cat, const string& note,
            const string& mode)
        : Transaction(id, d, amt, cat, note), paymentMode(mode) {}
    string type() const override { return "EXPENSE"; }
    double signedAmount() const override { return -amount; }
    string extra() const override { return paymentMode; }
    void display() const override {
        cout << left << setw(5) << id << setw(12) << date.str() << setw(9) << "Expense"
             << setw(15) << category << right << setw(11) << fixed << setprecision(2) << amount
             << left << "  " << setw(8) << paymentMode << note << "\n";
    }
};

class Income : public Transaction {
    string source;

public:
    Income(int id, const Date& d, double amt, const string& cat, const string& note,
           const string& src)
        : Transaction(id, d, amt, cat, note), source(src) {}
    string type() const override { return "INCOME"; }
    double signedAmount() const override { return amount; }
    string extra() const override { return source; }
    void display() const override {
        cout << left << setw(5) << id << setw(12) << date.str() << setw(9) << "Income"
             << setw(15) << category << right << setw(11) << fixed << setprecision(2) << amount
             << left << "  " << setw(8) << source << note << "\n";
    }
};

unique_ptr<Transaction> Transaction::fromLine(const string& line) {
    auto f = split(line, '|');
    if (f.size() < 7) throw ExpenseException("Corrupt transaction record.");
    int id = stoi(f[1]);
    Date d = Date::parse(f[2]);
    double amt = stod(f[3]);
    if (f[0] == "EXPENSE") return make_unique<Expense>(id, d, amt, f[4], f[5], f[6]);
    if (f[0] == "INCOME") return make_unique<Income>(id, d, amt, f[4], f[5], f[6]);
    throw ExpenseException("Unknown transaction type in file.");
}

// ======================================================== template Repository
template <typename T>
class Repository {
    vector<unique_ptr<T>> items;
    int next = 1;

public:
    int newId() { return next++; }
    void add(unique_ptr<T> item) {
        next = max(next, item->getId() + 1);
        items.push_back(std::move(item));
    }
    bool remove(int id) {
        auto it = find_if(items.begin(), items.end(),
                          [id](const unique_ptr<T>& p) { return p->getId() == id; });
        if (it == items.end()) return false;
        items.erase(it);
        return true;
    }
    T* find(int id) const {
        for (auto& p : items) if (p->getId() == id) return p.get();
        return nullptr;
    }
    const vector<unique_ptr<T>>& all() const { return items; }
    size_t size() const { return items.size(); }
};

// ================================================================== Summary
struct Summary {
    double income = 0, expense = 0;
    double net() const { return income - expense; }
    Summary operator+(const Summary& o) const { return {income + o.income, expense + o.expense}; }
};

// ================================================================== Tracker
class Tracker {
    Repository<Transaction> repo;
    map<string, double> budgets;        // category(lower) -> monthly limit
    map<string, string> displayName;    // lower -> original text

    static string key(const string& c) { return lower(c); }

    double spentIn(int y, int m, const string& cat, const Date& upTo) const {
        double total = 0;
        for (auto& t : repo.all())
            if (t->type() == "EXPENSE" && t->getDate().sameMonth(y, m) && t->getDate() <= upTo &&
                key(t->getCategory()) == key(cat))
                total += t->getAmount();
        return total;
    }

public:
    // ---- add / delete -------------------------------------------------------
    int addExpense(const Date& d, double amt, const string& cat, const string& note,
                   const string& mode) {
        int id = repo.newId();
        repo.add(make_unique<Expense>(id, d, amt, cat, note, mode));
        return id;
    }
    int addIncome(const Date& d, double amt, const string& cat, const string& note,
                  const string& src) {
        int id = repo.newId();
        repo.add(make_unique<Income>(id, d, amt, cat, note, src));
        return id;
    }
    void remove(int id) {
        if (!repo.remove(id)) throw NotFoundException("Transaction " + to_string(id));
    }

    // ---- UNIQUE FEATURE: budget guardian ------------------------------------
    void setBudget(const string& cat, double limit) {
        if (limit <= 0) throw InvalidAmountException("Budget must be greater than zero.");
        if (cat.empty()) throw ExpenseException("Category cannot be empty.");
        budgets[key(cat)] = limit;
        displayName[key(cat)] = cat;
    }
    void showBudgets() const {
        if (budgets.empty()) { cout << "No budgets set.\n"; return; }
        cout << "\n" << left << setw(18) << "Category" << "Monthly limit\n" << string(34, '-') << "\n";
        for (auto& kv : budgets)
            cout << setw(18) << displayName.at(kv.first) << money(kv.second) << "\n";
    }

    // Called right after an expense is added: instant 80% / 100% alerts.
    void checkAlert(const Date& d, const string& cat) const {
        auto it = budgets.find(key(cat));
        if (it == budgets.end()) return;
        double spent = spentIn(d.year(), d.month(), cat, Date(d.year(), d.month(), Date::daysInMonth(d.year(), d.month())));
        double pct = spent / it->second * 100.0;
        if (spent > it->second)
            cout << "  [!!] OVER BUDGET: " << displayName.at(key(cat)) << " spent " << money(spent) << " of "
                 << money(it->second) << " (over by " << money(spent - it->second) << ")\n";
        else if (pct >= 80.0)
            cout << "  [!] WARNING: " << displayName.at(key(cat)) << " has used " << fixed << setprecision(0) << pct
                 << "% of its budget (" << money(it->second - spent) << " left)\n";
    }

    void guardianDashboard(const Date& asOf) const {
        int y = asOf.year(), m = asOf.month();
        int dim = Date::daysInMonth(y, m);
        int elapsed = asOf.day();
        int remaining = dim - elapsed;

        Summary s;
        for (auto& t : repo.all())
            if (t->getDate().sameMonth(y, m) && t->getDate() <= asOf) {
                if (t->type() == "INCOME") s.income += t->getAmount();
                else s.expense += t->getAmount();
            }

        cout << "\n=========== BUDGET GUARDIAN  (as of " << asOf << ") ===========\n"
             << "Day " << elapsed << " of " << dim << "  |  " << remaining << " days left\n\n";
        if (budgets.empty()) cout << "No budgets set - use option 8 first.\n";
        else {
            cout << left << setw(14) << "Category" << right << setw(10) << "Limit" << setw(10)
                 << "Spent" << setw(11) << "Forecast" << setw(12) << "Safe/day" << "  Status\n"
                 << string(70, '-') << "\n";
        }
        int over = 0, atRisk = 0;
        for (auto& kv : budgets) {
            double spent = spentIn(y, m, kv.first, asOf);
            double forecast = spent / elapsed * dim;
            double safe = remaining > 0 ? max(0.0, kv.second - spent) / remaining : 0.0;
            string status;
            if (spent > kv.second) { status = "OVER"; ++over; }
            else if (forecast > kv.second) { status = "AT RISK"; ++atRisk; }
            else status = "On track";
            cout << left << setw(14) << displayName.at(kv.first) << right << fixed
                 << setprecision(0) << setw(10) << kv.second << setw(10) << spent << setw(11)
                 << forecast << setw(12) << safe << "  " << status << "\n";
        }

        double rate = s.income > 0 ? s.net() / s.income : 0.0;           // savings rate
        double score = 40 + max(-40.0, min(40.0, rate * 100.0)) - 10.0 * over - 5.0 * atRisk;
        score = max(0.0, min(100.0, score));
        string grade = score >= 80 ? "Excellent" : score >= 60 ? "Good" : score >= 40 ? "Fair" : "Poor";

        cout << "\nIncome this month   : " << money(s.income)
             << "\nExpenses this month : " << money(s.expense)
             << "\nNet savings         : " << money(s.net()) << "  (" << fixed << setprecision(1)
             << rate * 100 << "% of income)"
             << "\nBudgets over / at risk : " << over << " / " << atRisk
             << "\nFINANCIAL HEALTH SCORE : " << setprecision(0) << score << " / 100  (" << grade << ")\n"
             << "================================================================\n";
    }

    // ---- views ----------------------------------------------------------------
    void listAll() const {
        if (repo.size() == 0) { cout << "No transactions yet.\n"; return; }
        vector<const Transaction*> v;
        for (auto& t : repo.all()) v.push_back(t.get());
        sort(v.begin(), v.end(), [](const Transaction* a, const Transaction* b) {
            return a->getDate() < b->getDate() || (a->getDate() == b->getDate() && a->getId() < b->getId());
        });
        cout << "\n" << left << setw(5) << "ID" << setw(12) << "Date" << setw(9) << "Type" << setw(15)
             << "Category" << right << setw(11) << "Amount" << left << "  " << setw(8) << "Mode"
             << "Note\n" << string(75, '-') << "\n";
        for (auto* t : v) t->display();                     // polymorphic call
    }

    Summary monthSummary(int y, int m) const {
        Summary s;
        for (auto& t : repo.all())
            if (t->getDate().sameMonth(y, m)) {
                Summary one;
                if (t->type() == "INCOME") one.income = t->getAmount();
                else one.expense = t->getAmount();
                s = s + one;                                // operator+
            }
        return s;
    }

    void showMonthly(int y, int m) const {
        if (m < 1 || m > 12) throw InvalidDateException("Month must be 1-12.");
        Summary s = monthSummary(y, m);
        cout << "\n--- Summary for " << y << "-" << setfill('0') << setw(2) << m << setfill(' ')
             << " ---\nIncome   : " << money(s.income) << "\nExpenses : " << money(s.expense)
             << "\nNet      : " << money(s.net()) << "\n";
        map<string, double> byCat;
        for (auto& t : repo.all())
            if (t->type() == "EXPENSE" && t->getDate().sameMonth(y, m)) byCat[t->getCategory()] += t->getAmount();
        if (byCat.empty()) return;
        cout << "\nSpending by category:\n";
        vector<pair<string, double>> v(byCat.begin(), byCat.end());
        sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
        for (auto& kv : v) {
            double pct = kv.second / s.expense * 100.0;
            cout << left << setw(14) << kv.first << right << setw(10) << fixed << setprecision(2)
                 << kv.second << "  " << setw(5) << setprecision(1) << pct << "%  "
                 << string(static_cast<int>(pct / 4), '#') << "\n";
        }
        cout << "Top category: " << v.front().first << "\n";
    }

    void search(const string& key_) const {
        string k = lower(key_);
        bool any = false;
        for (auto& t : repo.all())
            if (lower(t->getCategory()).find(k) != string::npos || lower(t->getNote()).find(k) != string::npos) {
                t->display();
                any = true;
            }
        if (!any) cout << "No match.\n";
    }

    void exportCsv(const string& file) const {
        ofstream out(file);
        if (!out) throw ExpenseException("Cannot write " + file);
        out << "id,date,type,category,amount,note,mode_or_source\n";
        for (auto& t : repo.all())
            out << t->getId() << "," << t->getDate() << "," << t->type() << "," << t->getCategory()
                << "," << t->getAmount() << "," << t->getNote() << "," << t->extra() << "\n";
    }

    // ---- persistence ----------------------------------------------------------
    void save() const {
        ofstream t("transactions.txt"), b("budgets.txt");
        if (!t || !b) throw ExpenseException("Cannot open data files for writing.");
        for (auto& x : repo.all()) t << x->serialize() << "\n";
        for (auto& kv : budgets) b << displayName.at(kv.first) << "|" << kv.second << "\n";
    }
    void load() {
        string line;
        ifstream t("transactions.txt"), b("budgets.txt");
        while (getline(t, line)) if (!line.empty()) repo.add(Transaction::fromLine(line));
        while (getline(b, line)) {
            auto f = split(line, '|');
            if (f.size() == 2) setBudget(f[0], stod(f[1]));
        }
    }

    void loadDemo() {
        setBudget("Food", 5000);
        setBudget("Transport", 2000);
        setBudget("Shopping", 3000);
        setBudget("Entertainment", 1500);
        addIncome(Date(2026, 10, 1), 25000, "Salary", "Monthly pocket money / stipend", "Bank");
        addIncome(Date(2026, 10, 12), 3000, "Freelance", "Logo design", "UPI");
        const char* foods[] = {"Canteen", "Groceries", "Pizza night", "Cafe", "Dinner", "Snacks"};
        double fAmt[] = {450, 1200, 900, 600, 1100, 700};
        for (int i = 0; i < 6; ++i) addExpense(Date(2026, 10, 2 + i * 3), fAmt[i], "Food", foods[i], "UPI");
        addExpense(Date(2026, 10, 3), 600, "Transport", "Metro card", "Card");
        addExpense(Date(2026, 10, 9), 450, "Transport", "Cab", "UPI");
        addExpense(Date(2026, 10, 15), 1800, "Shopping", "Shoes", "Card");
        addExpense(Date(2026, 10, 18), 1500, "Shopping", "Headphones", "UPI");
        addExpense(Date(2026, 10, 14), 700, "Entertainment", "Movie + snacks", "Cash");
    }
};

// ============================================================ input helpers
string readLine(const string& p) {
    cout << p;
    string s;
    getline(cin, s);
    return s;
}
int readInt(const string& p) {
    while (true) {
        string s = readLine(p);
        try {
            size_t pos;
            int v = stoi(s, &pos);
            if (pos != s.size()) throw invalid_argument("x");
            return v;
        } catch (...) {
            if (cin.eof()) throw ExpenseException("Input ended.");
            cout << "  Please enter a valid whole number.\n";
        }
    }
}
double readDouble(const string& p) {
    while (true) {
        string s = readLine(p);
        try {
            size_t pos;
            double v = stod(s, &pos);
            if (pos != s.size()) throw invalid_argument("x");
            return v;
        } catch (...) {
            if (cin.eof()) throw ExpenseException("Input ended.");
            cout << "  Please enter a valid amount.\n";
        }
    }
}
Date readDate(const string& p, bool allowToday) {
    string s = readLine(p + (allowToday ? " (YYYY-MM-DD, Enter = today): " : " (YYYY-MM-DD): "));
    if (s.empty() && allowToday) return Date::today();
    return Date::parse(s);
}

void printMenu() {
    cout << "\n============= PERSONAL EXPENSE TRACKER =============\n"
         << "  1. Add expense              7. Set category budget\n"
         << "  2. Add income               8. View budgets\n"
         << "  3. List all transactions    9. BUDGET GUARDIAN dashboard\n"
         << "  4. Delete transaction      10. Export to CSV\n"
         << "  5. Monthly summary         11. Load demo data\n"
         << "  6. Search                   0. Save & exit\n"
         << "====================================================\n";
}

int main() {
    Tracker tr;
    try { tr.load(); } catch (const exception& e) { cout << "Load warning: " << e.what() << "\n"; }

    while (true) {
        printMenu();
        int ch;
        try { ch = readInt("Choice: "); } catch (const ExpenseException&) { ch = 0; }
        try {
            switch (ch) {
                case 0: tr.save(); cout << "Data saved. Goodbye!\n"; return 0;
                case 1: {
                    Date d = readDate("Date", true);
                    double amt = readDouble("Amount: ");
                    string cat = readLine("Category (Food/Transport/...): ");
                    string note = readLine("Note: ");
                    string mode = readLine("Payment mode (Cash/UPI/Card): ");
                    int id = tr.addExpense(d, amt, cat, note, mode);
                    cout << "Expense added with ID " << id << "\n";
                    tr.checkAlert(d, cat);
                    break;
                }
                case 2: {
                    Date d = readDate("Date", true);
                    double amt = readDouble("Amount: ");
                    string cat = readLine("Category (Salary/Gift/...): ");
                    string note = readLine("Note: ");
                    string src = readLine("Source: ");
                    int id = tr.addIncome(d, amt, cat, note, src);
                    cout << "Income added with ID " << id << "\n";
                    break;
                }
                case 3: tr.listAll(); break;
                case 4: tr.remove(readInt("Transaction ID: ")); cout << "Deleted.\n"; break;
                case 5: {
                    int y = readInt("Year: ");
                    int m = readInt("Month (1-12): ");
                    tr.showMonthly(y, m);
                    break;
                }
                case 6: tr.search(readLine("Search text: ")); break;
                case 7: {
                    string cat = readLine("Category: ");
                    double lim = readDouble("Monthly limit: ");
                    tr.setBudget(cat, lim);
                    cout << "Budget saved.\n";
                    break;
                }
                case 8: tr.showBudgets(); break;
                case 9: tr.guardianDashboard(readDate("As-of date", true)); break;
                case 10: tr.exportCsv("expenses_export.csv"); cout << "Exported to expenses_export.csv\n"; break;
                case 11: tr.loadDemo(); cout << "Demo data loaded (October 2026).\n"; break;
                default: cout << "Invalid choice.\n";
            }
        } catch (const ExpenseException& e) {
            cout << "  [x] " << e.what() << "\n";
        } catch (const exception& e) {
            cout << "  [x] Unexpected error: " << e.what() << "\n";
        }
    }
}
