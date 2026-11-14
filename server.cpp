#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <map>
#include "httplib.h"

using namespace std;

// ============================================================================
// DATA STRUCTURE 1: Singly Linked List for Card Discounts (CardDealNode)
// ============================================================================
struct CardDealNode {
    string provider;       // HBL, Meezan, JazzCash, Easypaisa, etc.
    string providerType;   // "Bank" or "Digital Wallet"
    string cardTier;       // e.g., "Visa Platinum", "World Debit"
    int discountPct;
    int maxCapPKR;
    int minSpendPKR;
    string validDays;
    string terms;
    CardDealNode* next;

    CardDealNode(string prov, string pType, string tier,
                 int pct, int cap, int minSp, string days, string trms)
        : provider(prov), providerType(pType), cardTier(tier),
          discountPct(pct), maxCapPKR(cap), minSpendPKR(minSp),
          validDays(days), terms(trms), next(nullptr) {}
};

// Restaurant Entity (Also acts as a Chaining Node inside the Hash Table)
struct RestaurantNode {
    string name;
    string category;
    string city;
    CardDealNode* dealsHead;    // Head pointer to Singly Linked List of deals
    RestaurantNode* nextInBucket;

    RestaurantNode(string n, string cat, string c)
        : name(n), category(cat), city(c), dealsHead(nullptr), nextInBucket(nullptr) {}

    void addCardDeal(string prov, string pType, string tier,
                     int pct, int cap, int minSp, string days, string trms) {
        CardDealNode* newNode = new CardDealNode(prov, pType, tier, pct, cap, minSp, days, trms);
        newNode->next = dealsHead;
        dealsHead = newNode;
    }

    ~RestaurantNode() {
        CardDealNode* curr = dealsHead;
        while (curr != nullptr) {
            CardDealNode* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }
};

// ============================================================================
// DATA STRUCTURE 2: Binary Search Tree (BST) Ordered by Max Discount %
// Supports: Reverse In-Order Traversal (High-to-Low) & Pruned Range Search
// ============================================================================
struct BSTNode {
    int maxDiscountKey;
    string jsonPayload;
    BSTNode* left;
    BSTNode* right;

    BSTNode(int key, const string& payload)
        : maxDiscountKey(key), jsonPayload(payload), left(nullptr), right(nullptr) {}
};

class DiscountBST {
private:
    BSTNode* root;

    BSTNode* insertRec(BSTNode* node, int key, const string& payload) {
        if (node == nullptr) return new BSTNode(key, payload);
        // Duplicate keys go to the right subtree so stable ordering is preserved
        if (key < node->maxDiscountKey)
            node->left = insertRec(node->left, key, payload);
        else
            node->right = insertRec(node->right, key, payload);
        return node;
    }

    // Reverse In-Order (Right -> Root -> Left) with BST Range Pruning!
    void rangeSearchHighToLow(BSTNode* node, int minDiscountPct, ostringstream& out, bool& first) const {
        if (node == nullptr) return;

        // Always visit right subtree (higher discounts)
        rangeSearchHighToLow(node->right, minDiscountPct, out, first);

        // Process current node if it meets the threshold
        if (node->maxDiscountKey >= minDiscountPct) {
            if (!first) out << ",";
            first = false;
            out << node->jsonPayload;

            // Only explore left subtree if current key >= minDiscountPct
            // If current key < minDiscountPct, everything in left is even smaller (PRUNED!)
            rangeSearchHighToLow(node->left, minDiscountPct, out, first);
        }
        // else: PRUNE entire left subtree — all keys there are < minDiscountPct
    }

    void destroyRec(BSTNode* node) {
        if (!node) return;
        destroyRec(node->left);
        destroyRec(node->right);
        delete node;
    }

public:
    DiscountBST() : root(nullptr) {}
    ~DiscountBST() { destroyRec(root); }

    void insert(int maxDiscountKey, const string& jsonPayload) {
        root = insertRec(root, maxDiscountKey, jsonPayload);
    }

    string getSortedJSON(int minDiscountPct) const {
        ostringstream out;
        out << "[";
        bool first = true;
        rangeSearchHighToLow(root, minDiscountPct, out, first);
        out << "]";
        return out.str();
    }
};

// ============================================================================
// DATA STRUCTURE 3: Fixed-Capacity Circular Array Queue (Recent Search Log)
// ============================================================================
class RecentSearchQueue {
private:
    static const int CAPACITY = 5;
    string arr[CAPACITY];
    int frontIdx;
    int rearIdx;
    int count;

public:
    RecentSearchQueue() : frontIdx(0), rearIdx(-1), count(0) {}

    void enqueue(const string& searchDesc) {
        // Avoid pushing identical consecutive searches
        if (count > 0 && arr[rearIdx] == searchDesc) return;

        if (count == CAPACITY) {
            // Overwrite oldest entry by advancing frontIdx (Circular Queue behavior)
            frontIdx = (frontIdx + 1) % CAPACITY;
            count--;
        }
        rearIdx = (rearIdx + 1) % CAPACITY;
        arr[rearIdx] = searchDesc;
        count++;
    }

    string toJSON() const {
        ostringstream out;
        out << "[";
        for (int i = 0; i < count; ++i) {
            // Read from newest (rear) to oldest (front)
            int idx = (rearIdx - i + CAPACITY) % CAPACITY;
            if (i > 0) out << ",";
            // Escape double quotes in the search description
            out << "\"";
            for (char c : arr[idx]) {
                if (c == '"') out << "\\\"";
                else if (c == '\\') out << "\\\\";
                else out << c;
            }
            out << "\"";
        }
        out << "]";
        return out.str();
    }
};

// ============================================================================
// DATA STRUCTURE 4: Hash Table + Sorted Index Array for Binary Search
// ============================================================================
class DiscountEngine {
private:
    static const int TABLE_SIZE = 503;
    static const int MAX_RESTAURANTS = 50000;

    RestaurantNode* buckets[TABLE_SIZE];              // Hash Table Buckets
    RestaurantNode* sortedDirectory[MAX_RESTAURANTS];  // Sorted Array for O(log N) Binary Search
    int totalRestaurants;
    RecentSearchQueue searchHistory;

    // Polynomial Rolling Hash Function: O(L) where L is city name length
    int hashCity(const string& city) const {
        unsigned long hash = 0;
        for (char c : city) {
            char lower = tolower(static_cast<unsigned char>(c));
            hash = (hash * 31 + lower) % TABLE_SIZE;
        }
        return static_cast<int>(hash);
    }

    // Helper: Case-insensitive substring search (used by Linear Search)
    bool containsSubstringCI(const string& haystack, const string& needle) const {
        if (needle.empty()) return true;
        string hLower, nLower;
        for (char c : haystack) hLower += tolower(static_cast<unsigned char>(c));
        for (char c : needle)   nLower += tolower(static_cast<unsigned char>(c));
        return hLower.find(nLower) != string::npos;
    }

    string escapeJSON(const string& s) const {
        string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c != '\r' && c != '\n') out += c;
        }
        return out;
    }

    void addToDirectory(RestaurantNode* newRest) {
        if (totalRestaurants < MAX_RESTAURANTS) {
            sortedDirectory[totalRestaurants++] = newRest;
        }
    }

    void sortDirectory() {
        sort(sortedDirectory, sortedDirectory + totalRestaurants,
             [](RestaurantNode* a, RestaurantNode* b) {
                 if (!a && !b) return false;
                 if (!a) return false;
                 if (!b) return true;
                 return a->name < b->name;
             });
    }

    // ========================================================================
    // SEARCH METHOD 2: Iterative Binary Search on Sorted Pointer Array O(log N)
    // Used when the user picks a specific restaurant name from dropdown!
    // ========================================================================
    int binarySearchRestaurantFirstIndex(const string& targetName, int& comparisonsOut) const {
        int low = 0;
        int high = totalRestaurants - 1;
        int resultIdx = -1;
        comparisonsOut = 0;

        while (low <= high) {
            comparisonsOut++;
            int mid = low + (high - low) / 2;
            if (sortedDirectory[mid]->name == targetName) {
                resultIdx = mid;
                high = mid - 1; // Keep searching left for first occurrence
            } else if (sortedDirectory[mid]->name < targetName) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        return resultIdx;
    }

    // Serializes a RestaurantNode and its matching CardDealNode linked list into JSON,
    // Groups cards with identical (provider + discount% + cap + minSpend + validDays) into one deal node.
    // Then inserts into the DiscountBST keyed by max discount %.
    void evaluateAndInsertIntoBST(RestaurantNode* rNode,
                                  const string& providerFilter,
                                  const string& cardFilter,
                                  const string& keywordSearch,
                                  DiscountBST& bst,
                                  int& matchedRestaurantsCount) const {

        // Check if restaurant name or category matches the keyword (Linear Search component)
        bool restMatchesKeyword = containsSubstringCI(rNode->name, keywordSearch) ||
                                  containsSubstringCI(rNode->category, keywordSearch);

        // ---- PASS 1: collect matching deals, grouped by (provider|pct|cap|minSpend|days) ----
        // key   -> "provider|pct|cap|minSpend|days|terms"
        // value -> list of card names
        map<string, vector<string>> groupedCards;   // key -> card names
        // Metadata per group key (we just need one representative node)
        map<string, CardDealNode*> groupMeta;

        CardDealNode* dCurr = rNode->dealsHead;
        while (dCurr != nullptr) {
            bool provMatch = (providerFilter.empty() || providerFilter == "All" || dCurr->provider == providerFilter);
            bool cardMatch = (cardFilter.empty() || cardFilter == "All" ||
                              dCurr->cardTier == cardFilter ||
                              containsSubstringCI(dCurr->cardTier, cardFilter));
            bool dealMatchesKeyword = restMatchesKeyword ||
                                      containsSubstringCI(dCurr->provider, keywordSearch) ||
                                      containsSubstringCI(dCurr->cardTier, keywordSearch);

            if (provMatch && cardMatch && dealMatchesKeyword) {
                // Build grouping key — same discount terms = same display node
                string groupKey = dCurr->provider + "|"
                                + to_string(dCurr->discountPct) + "|"
                                + to_string(dCurr->maxCapPKR) + "|"
                                + to_string(dCurr->minSpendPKR) + "|"
                                + dCurr->validDays;

                groupedCards[groupKey].push_back(dCurr->cardTier);
                if (groupMeta.find(groupKey) == groupMeta.end()) {
                    groupMeta[groupKey] = dCurr;
                }
            }
            dCurr = dCurr->next;
        }

        if (groupedCards.empty()) return;

        // ---- PASS 2: serialize one JSON deal object per group ----
        ostringstream dealsStream;
        bool firstDeal = true;
        int dealCount = 0;
        int highestPct = 0;

        for (auto& kv : groupedCards) {
            const string& groupKey = kv.first;
            vector<string>& cardNames = kv.second;
            CardDealNode* meta = groupMeta[groupKey];

            if (meta->discountPct > highestPct) highestPct = meta->discountPct;
            dealCount++;

            if (!firstDeal) dealsStream << ",";
            firstDeal = false;

            // Build the cards[] JSON array — deduplicated
            vector<string> seen;
            ostringstream cardsArr;
            cardsArr << "[";
            bool firstCard = true;
            for (const string& cn : cardNames) {
                // Simple dedup: only add if not already seen (case-insensitive)
                bool dup = false;
                for (const string& s : seen) {
                    if (s == cn) { dup = true; break; }
                }
                if (!dup) {
                    seen.push_back(cn);
                    if (!firstCard) cardsArr << ",";
                    firstCard = false;
                    cardsArr << "\"" << escapeJSON(cn) << "\"";
                }
            }
            cardsArr << "]";

            dealsStream << "{"
                        << "\"provider\":\"" << escapeJSON(meta->provider) << "\","
                        << "\"provider_type\":\"" << escapeJSON(meta->providerType) << "\","
                        << "\"cards\":" << cardsArr.str() << ","
                        << "\"discount_pct\":" << meta->discountPct << ","
                        << "\"max_cap_pkr\":" << meta->maxCapPKR << ","
                        << "\"min_spend_pkr\":" << meta->minSpendPKR << ","
                        << "\"valid_days\":\"" << escapeJSON(meta->validDays) << "\","
                        << "\"terms\":\"" << escapeJSON(meta->terms) << "\""
                        << "}";
        }

        matchedRestaurantsCount++;
        ostringstream restJSON;
        restJSON << "{"
                 << "\"restaurant\":\"" << escapeJSON(rNode->name) << "\","
                 << "\"category\":\"" << escapeJSON(rNode->category) << "\","
                 << "\"city\":\"" << escapeJSON(rNode->city) << "\","
                 << "\"max_discount\":" << highestPct << ","
                 << "\"deal_count\":" << dealCount << ","
                 << "\"deals\":[" << dealsStream.str() << "]"
                 << "}";

        bst.insert(highestPct, restJSON.str());
    }

public:
    DiscountEngine() : totalRestaurants(0) {
        for (int i = 0; i < TABLE_SIZE; ++i) buckets[i] = nullptr;
        for (int i = 0; i < MAX_RESTAURANTS; ++i) sortedDirectory[i] = nullptr;
    }

    RestaurantNode* getOrCreateRestaurant(const string& name, const string& category, const string& city) {
        int bucketIdx = hashCity(city);
        RestaurantNode* curr = buckets[bucketIdx];

        while (curr != nullptr) {
            if (curr->city == city && curr->name == name) return curr;
            curr = curr->nextInBucket;
        }

        RestaurantNode* newRest = new RestaurantNode(name, category, city);
        newRest->nextInBucket = buckets[bucketIdx];
        buckets[bucketIdx] = newRest;

        // Also register in sorted pointer array for Binary Search
        addToDirectory(newRest);
        return newRest;
    }

    int safeParseInt(const string& str, int fallback = 0) const {
        try {
            if (str.empty()) return fallback;
            return stoi(str);
        } catch (...) {
            return fallback;
        }
    }

    vector<string> parseCSVLine(const string& line) const {
        vector<string> fields;
        string field;
        bool inQuotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = !inQuotes;
                }
            } else if (c == ',' && !inQuotes) {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(field);
        return fields;
    }

    bool loadFromCSV(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) return false;

        string line;
        getline(file, line); // Skip header

        int dealRows = 0;
        while (getline(file, line)) {
            if (line.empty()) continue;

            // Handle \r\n line endings on Windows
            if (!line.empty() && line.back() == '\r') line.pop_back();

            vector<string> cols = parseCSVLine(line);
            if (cols.size() < 11) continue;

            string rest   = cols[0];
            string cat    = cols[1];
            string city   = cols[2];
            string prov   = cols[3];
            string pType  = cols[4];
            string tier   = cols[5];
            string pctStr = cols[6];
            string capStr = cols[7];
            string minStr = cols[8];
            string days   = cols[9];
            string terms  = cols[10];

            RestaurantNode* rNode = getOrCreateRestaurant(rest, cat, city);
            int pct = safeParseInt(pctStr, 0);
            int cap = safeParseInt(capStr, 0);
            int minSp = safeParseInt(minStr, 0);
            rNode->addCardDeal(prov, pType, tier, pct, cap, minSp, days, terms);
            dealRows++;
        }

        // Sort directory once after all restaurants are loaded: O(N log N)
        sortDirectory();

        cout << "[DSA Engine] Loaded " << dealRows << " deals across "
             << totalRestaurants << " unique restaurants." << endl;
        cout << "[DSA Engine] Hash Table Buckets: " << TABLE_SIZE
             << " | Sorted Directory Size: " << totalRestaurants << endl;
        return true;
    }

    // ========================================================================
    // DYNAMIC QUERY ROUTER: Picks the optimal Search Algorithm automatically!
    // ========================================================================
    string executeSmartQuery(const string& cityFilter,
                             const string& providerFilter,
                             const string& cardFilter,
                             const string& restFilter,
                             const string& keywordSearch,
                             int minDiscountPct) {
        DiscountBST resultBST;
        int matchedCount = 0;
        string activeSearchAlgo;

        // =====================================================================
        // CASE 1: User selected a specific Restaurant
        //         -> Use O(log N) BINARY SEARCH on sorted pointer array!
        // =====================================================================
        if (!restFilter.empty() && restFilter != "All") {
            int comparisons = 0;
            int idx = binarySearchRestaurantFirstIndex(restFilter, comparisons);

            activeSearchAlgo = "Iterative Binary Search O(log N) on Sorted Array (" +
                               to_string(comparisons) + " comparisons across " +
                               to_string(totalRestaurants) + " restaurants)";

            // Walk forward from the found index to collect all entries with the same name
            // (a restaurant can appear in multiple cities)
            while (idx != -1 && idx < totalRestaurants && sortedDirectory[idx]->name == restFilter) {
                RestaurantNode* rNode = sortedDirectory[idx];
                if (cityFilter.empty() || cityFilter == "All" || rNode->city == cityFilter) {
                    evaluateAndInsertIntoBST(rNode, providerFilter, cardFilter, keywordSearch, resultBST, matchedCount);
                }
                idx++;
            }
            searchHistory.enqueue("Binary Search: " + restFilter);
        }
        // =====================================================================
        // CASE 2: User selected a specific City (no specific restaurant)
        //         -> Use O(1) HASH TABLE BUCKET LOOKUP!
        // =====================================================================
        else if (!cityFilter.empty() && cityFilter != "All") {
            int bucketIdx = hashCity(cityFilter);
            activeSearchAlgo = "Direct Hash Table Lookup O(1) [Bucket #" + to_string(bucketIdx) + "]";

            if (!keywordSearch.empty()) {
                activeSearchAlgo += " + Substring Linear Scan O(N*K)";
                searchHistory.enqueue("City Hash + Keyword: " + cityFilter + " ('" + keywordSearch + "')");
            } else if (providerFilter != "All" && !providerFilter.empty()) {
                searchHistory.enqueue(cityFilter + " -> " + providerFilter);
            } else {
                searchHistory.enqueue("Hash Lookup: " + cityFilter);
            }

            RestaurantNode* rCurr = buckets[bucketIdx];
            while (rCurr != nullptr) {
                if (rCurr->city == cityFilter) {
                    evaluateAndInsertIntoBST(rCurr, providerFilter, cardFilter, keywordSearch, resultBST, matchedCount);
                }
                rCurr = rCurr->nextInBucket;
            }
        }
        // =====================================================================
        // CASE 3: No city, no specific restaurant
        //         -> Full Hash Table Traversal + Linear Substring Search
        // =====================================================================
        else {
            activeSearchAlgo = keywordSearch.empty()
                ? "Full Hash Table Chaining Traversal O(N + K)"
                : "Global Substring Linear Search O(N*K) for '" + escapeJSON(keywordSearch) + "'";

            if (!keywordSearch.empty()) {
                searchHistory.enqueue("Linear Search: '" + keywordSearch + "'");
            }

            for (int i = 0; i < TABLE_SIZE; ++i) {
                RestaurantNode* rCurr = buckets[i];
                while (rCurr != nullptr) {
                    evaluateAndInsertIntoBST(rCurr, providerFilter, cardFilter, keywordSearch, resultBST, matchedCount);
                    rCurr = rCurr->nextInBucket;
                }
            }
        }

        // Append BST sort/prune info
        if (minDiscountPct > 0) {
            activeSearchAlgo += " + BST Range Pruning (>= " + to_string(minDiscountPct) + "% OFF)";
        } else {
            activeSearchAlgo += " + BST Reverse In-Order Sort (High->Low)";
        }

        // Build final JSON envelope containing telemetry + results + circular queue history
        ostringstream response;
        response << "{"
                 << "\"algorithm_used\":\"" << escapeJSON(activeSearchAlgo) << "\","
                 << "\"matched_count\":" << matchedCount << ","
                 << "\"recent_searches\":" << searchHistory.toJSON() << ","
                 << "\"restaurants\":" << resultBST.getSortedJSON(minDiscountPct)
                 << "}";
        return response.str();
    }

    void clear() {
        for (int i = 0; i < TABLE_SIZE; ++i) {
            RestaurantNode* curr = buckets[i];
            while (curr != nullptr) {
                RestaurantNode* temp = curr;
                curr = curr->nextInBucket;
                delete temp;
            }
            buckets[i] = nullptr;
        }
        for (int i = 0; i < MAX_RESTAURANTS; ++i) {
            sortedDirectory[i] = nullptr;
        }
        totalRestaurants = 0;
    }

    ~DiscountEngine() {
        clear();
    }
};

// ============================================================================
// HTTP SERVER ENTRYPOINT
// ============================================================================
int main() {
    DiscountEngine engine;
    if (!engine.loadFromCSV("deals_data.csv")) {
        cerr << "[Error] deals_data.csv missing! Run 'python scraper.py' first.\n";
        return 1;
    }

    httplib::Server svr;
    svr.set_mount_point("/", "./public");

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // GET /api/restaurants?city=...&provider=...&card=...&restaurant=...&q=...&min_pct=...
    svr.Get("/api/restaurants", [&](const httplib::Request& req, httplib::Response& res) {
        string city = req.has_param("city") ? req.get_param_value("city") : "All";
        string provider = req.has_param("provider") ? req.get_param_value("provider") : "All";
        string card = req.has_param("card") ? req.get_param_value("card") : "All";
        string restaurant = req.has_param("restaurant") ? req.get_param_value("restaurant") : "All";
        string keyword = req.has_param("q") ? req.get_param_value("q") : "";
        int minPct = 0;
        if (req.has_param("min_pct")) {
            try { minPct = stoi(req.get_param_value("min_pct")); }
            catch (...) { minPct = 0; }
        }

        string payload = engine.executeSmartQuery(city, provider, card, restaurant, keyword, minPct);
        res.set_content(payload, "application/json");
    });

    // POST /api/scrape-live - Triggers live Playwright scraper and reloads data structures
    svr.Post("/api/scrape-live", [&](const httplib::Request&, httplib::Response& res) {
        cout << "[C++ Server] Triggering live Python Playwright scraper...\n";
        int status = system("python live_scraper.py");
        if (status != 0) {
            status = system("python3 live_scraper.py");
        }
        if (status == 0) {
            // Reload freshly scraped CSV into C++ Data Structures
            engine.clear();
            engine.loadFromCSV("deals_data.csv");
            res.set_content("{\"status\":\"success\",\"message\":\"Live bank data scraped and loaded into C++ Trie, Hash Table & Max-Heap!\"}", "application/json");
        } else {
            res.set_content("{\"status\":\"error\",\"message\":\"Live scraper process failed or encountered an error.\"}", "application/json");
        }
    });

    // POST /api/reload - Re-run seeded Python scraper
    svr.Post("/api/reload", [&](const httplib::Request&, httplib::Response& res) {
        int code = system("python scraper.py");
        if (code != 0) code = system("python3 scraper.py");
        if (code == 0) {
            engine.clear();
            engine.loadFromCSV("deals_data.csv");
        }
        res.set_content("{\"status\":\"reloaded\",\"code\":" + to_string(code) + "}", "application/json");
    });

    // Health check endpoint for cloud orchestrators (Render, Railway, Fly.io, Kubernetes, AWS, etc.)
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"healthy\",\"service\":\"PakSaver-DSA\"}", "application/json");
    });

    // Cloud deployment port: reads $PORT env var (standard on Render, Railway, Fly, Heroku, etc.)
    int port = 8080;
    const char* envPort = getenv("PORT");
    if (envPort && *envPort) {
        try { port = stoi(envPort); } catch (...) { port = 8080; }
    }

    cout << "==============================================================" << endl;
    cout << " PakSaver Multi-DSA C++ Server Live at http://localhost:" << port << endl;
    cout << " Hash Table: 503 Buckets | Max Directory: 50000 Restaurants" << endl;
    cout << " Data Structures: Hash Table | Linked List | BST | Queue" << endl;
    cout << " Search Methods:  O(1) Hash | O(logN) Binary | O(N*K) Linear" << endl;
    cout << "==============================================================" << endl;
    if (!svr.listen("0.0.0.0", port)) {
        cerr << "[Error] Server failed to start on port " << port << "! Port might already be in use." << endl;
        return 1;
    }
    return 0;
}
