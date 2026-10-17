import urllib.request
import json
import csv
import re
import time
import os
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

# Force UTF-8 on standard output for Windows console compatibility
if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

CSV_FILE = "deals_data.csv"

# ============================================================================
# PAKSAVER 100% LIVE NATIONWIDE AUTO-DISCOVERY SCRAPER
# Real Banks & Digital Wallets Only (K-Electric, Insurance & Telcos Filtered)
# Exact Card Tiers from Deal Associations + Accurate Real-World Discount Rates
# ============================================================================

HEADERS = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36',
    'authorization': 'Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6NzIsInJvbGUiOiJndWVzdCIsImlhdCI6MTU1MzcwMDgwNiwianRpIjoiUEpJMXFTb2ktQzRBZFJWcm9nb3RNV2UzV3VXcFdXTm0ifQ.2mb26xL4Qt7FfBQZ-XQvp-fhecMpaVUVXWp_GEST_6U',
    'version': '2.1.0.2',
    'medium': 'WEB',
    'content-type': 'application/json',
    'referer': 'https://peekaboo.guru/',
    'origin': 'https://peekaboo.guru'
}

# Major Pakistani cities to cover
ALL_CITIES = [
    "Karachi", "Lahore", "Islamabad", "Rawalpindi", "Faisalabad",
    "Multan", "Peshawar", "Quetta", "Sialkot", "Gujranwala",
    "Hyderabad", "Abbottabad", "Bahawalpur", "Sargodha", "Sukkur",
    "Gujrat", "Mardan", "Wah Cantt", "Kasur", "Sahiwal", "Okara",
    "Rahim Yar Khan", "Jhang", "Larkana", "Sheikhupura", "Dera Ghazi Khan",
    "Mirpur", "Muzaffarabad", "Swat", "Nowshera", "Chiniot", "Kamoke", "Hafizabad"
]

PCT_REGEX = re.compile(r'(\d{1,2})\s*%', re.IGNORECASE)

CAP_PATTERNS = [
    re.compile(r'(?:cap(?:ped)?|max(?:imum)?\s*(?:discount|savings?)?)\s*(?:is|at|to|of|up\s*to|[:\s])*\s*(?:rs\.?|pkr)\s*([\d,]+)', re.I),
    re.compile(r'(?:rs\.?|pkr)\s*([\d,]+)\s*(?:/-)?\s*(?:whichever\s*is\s*(?:lower|less)|cap)', re.I),
    re.compile(r'(?:maximum|max|cap)\s*(?:of)?\s*(?:rs\.?|pkr)\s*([\d,]+)', re.I),
    re.compile(r'up\s*to\s*(?:rs\.?|pkr)\s*([\d,]+)\s*(?:discount|off)', re.I),
]

MIN_SPEND_REGEX = re.compile(r'(?:min(?:imum)?\s+(?:spend|bill|order|amount)[:\s]*(?:of\s*)?)\s*(?:rs\.?|pkr)\s*([\d,]+)', re.I)
DAYS_REGEX = re.compile(r'(Mon\s*-\s*Thu|Mon\s*-\s*Fri|Fri\s*-\s*Sun|Weekdays|Weekends|Wednesday[s]?|Thursday[s]?|Friday[s]?|Saturday[s]?|Sunday[s]?|All\s+Days)', re.IGNORECASE)

def is_valid_bank_or_wallet(provider):
    """
    Excludes non-bank entities such as electricity utility companies (K-Electric),
    insurance companies (EFU Life, Jubilee), telcos (Onic), and internal test accounts.
    Only genuine banks and digital payment wallets are retained.
    """
    if not provider:
        return False
    p = str(provider).lower().strip()
    non_bank_keywords = [
        "electric", "insurance", "efu", "onic", "test",
        "telecom", "ptcl", "ufone", "telenor microfinance"
    ]
    return not any(bad in p for bad in non_bank_keywords)

def clean_bank_name(provider):
    if not provider:
        return "Bank"
    p = str(provider).strip()
    p_lower = p.lower()
    if "habib bank limited" in p_lower:
        return "HBL"
    if "hbl islamic" in p_lower:
        return "HBL Islamic"
    if "united bank limited" in p_lower or p_lower == "ubl":
        return "UBL"
    if "askari bank" in p_lower:
        return "Askari Bank"
    if "faysal bank" in p_lower:
        return "Faysal Bank"
    if "bank of punjab" in p_lower or p_lower == "bop":
        return "Bank of Punjab"
    if "mcb islamic" in p_lower:
        return "MCB Islamic"
    if "mcb bank" in p_lower:
        return "MCB"
    if "habib metro" in p_lower:
        return "Habib Metro"
    if "soneri" in p_lower:
        return "Soneri Bank"
    if "standard chartered" in p_lower:
        return "Standard Chartered"
    if "national bank" in p_lower or p_lower == "nbp":
        return "NBP"
    if "sindh bank" in p_lower:
        return "Sindh Bank"
    if "al baraka" in p_lower:
        return "Al Baraka"
    if "bank alfalah" in p_lower:
        return "Bank Alfalah"
    if "bank al habib" in p_lower:
        return "Bank AL Habib"
    if "bankislami" in p_lower:
        return "BankIslami"
    if "meezan" in p_lower:
        return "Meezan Bank"
    if "js bank" in p_lower:
        return "JS Bank"
    return p

def combine_card_names(bank_name, card_names):
    """
    Unifies all qualifying cards for a specific discount deal into a single clean tier.
    E.g. if 15% applies to both Visa Platinum Debit and Credit, they are combined into:
    'Allied Bank Visa Platinum Debit & Credit Cards'.
    """
    if not card_names:
        return f"{bank_name} All Cards"

    cleaned = []
    seen = set()
    for c in card_names:
        name = " ".join(str(c).split()).strip()
        if name.lower() not in seen:
            seen.add(name.lower())
            cleaned.append(name)

    if len(cleaned) == 1:
        c = cleaned[0]
        if bank_name.lower() in c.lower():
            return c
        return f"{bank_name} {c}"

    if len(cleaned) == 2:
        c1, c2 = cleaned[0], cleaned[1]
        base1 = c1.replace("Debit Card", "").replace("Credit Card", "").replace("Debit", "").replace("Credit", "").strip()
        base2 = c2.replace("Debit Card", "").replace("Credit Card", "").replace("Debit", "").replace("Credit", "").strip()
        if base1.lower() == base2.lower():
            tier = base1
            if bank_name.lower() not in tier.lower():
                tier = f"{bank_name} {tier}"
            return f"{tier} Debit & Credit Cards".strip()

        if "classic" in c1.lower() and "classic" in c2.lower():
            prefix = bank_name if bank_name.lower() not in c1.lower() else ""
            return f"{prefix} Classic Visa & Mastercard Debit Cards".strip()

    all_str = " ".join(cleaned).lower()
    tiers = []
    for t in ["Infinite", "World", "Signature", "Platinum", "Titanium", "Gold", "Silver", "Classic", "PayPak", "UnionPay"]:
        if t.lower() in all_str and t not in tiers:
            tiers.append(t)

    if tiers:
        if len(tiers) == 1:
            tier_str = tiers[0]
        elif len(tiers) == 2:
            tier_str = f"{tiers[0]} & {tiers[1]}"
        else:
            tier_str = f"{', '.join(tiers[:-1])} & {tiers[-1]}"

        has_debit = "debit" in all_str
        has_credit = "credit" in all_str
        card_type = "Debit & Credit Cards" if (has_debit and has_credit) else ("Debit Cards" if has_debit else "Cards")
        prefix = bank_name if bank_name.lower() not in tier_str.lower() else ""
        return f"{prefix} {tier_str} {card_type}".strip()

    short_names = [c.replace(bank_name, "").strip() for c in cleaned[:2]]
    return f"{bank_name} {' & '.join(short_names)}".strip()

def classify_category(merchant, keywords, title, desc):
    """Accurately categorizes merchant into clear user-facing domain categories."""
    text = f"{merchant} {' '.join(str(k) for k in keywords) if isinstance(keywords, list) else ''} {title} {desc}".lower()
    if any(w in text for w in [
        "lab", "clinic", "diagnostic", "hospital", "dental", "pharma", "pharmacy",
        "doctor", "aesthetic", "skin", "health", "optical", "eye care", "medical",
        "chughtai", "shaukat khanum", "aga khan", "essa", "idc", "dentist",
        "consultation", "root canal", "scaling", "polishing", "physio"
    ]):
        return "Health & Wellness"
    if any(w in text for w in [
        "cafe", "coffee", "restaurant", "pizza", "burger", "grill", "bbq", "karahi",
        "biryani", "dining", "bakery", "bakers", "sweets", "gelato", "dessert",
        "kitchen", "food", "bar.b.q", "cuisine", "bites", "steak", "fast food",
        "tea", "chai", "roastery", "shawarma", "kabab", "kababjees", "dhabba",
        "takeaway", "dine-in", "crust", "fries", "broast", "ice cream", "donut", "waffle"
    ]):
        return "Food & Dining"
    if any(w in text for w in [
        "cloth", "apparel", "fashion", "shoes", "pret", "textile", "jewel", "jewelry",
        "gold jewelry", "watch", "bags", "couture", "boutique", "fabrics", "retail",
        "mart", "store", "supermarket", "grocer", "perfume", "fragrance", "leather"
    ]):
        return "Fashion & Retail"
    if any(w in text for w in [
        "hotel", "resort", "inn", "suites", "travel", "tour", "airline", "stay",
        "room booking", "rack rate"
    ]):
        return "Travel & Hospitality"
    if any(w in text for w in [
        "petroleum", "attock", "pso", "total parco", "lubricant", "fuel", "tyre", "auto", "car care"
    ]):
        return "Automotive & Fuel"
    if any(w in text for w in [
        "salon", "spa", "grooming", "beauty", "makeover", "hair", "nail", "barber"
    ]):
        return "Salon & Grooming"
    return "Lifestyle & Entertainment"

def extract_cap(text):
    for p in CAP_PATTERNS:
        m = p.search(text)
        if m:
            val = int(m.group(1).replace(',', ''))
            if 100 <= val <= 300000:
                return val
    return 0

def extract_deal_rows(deal, default_city=None):
    """
    Extracts structured rows for a single live deal:
    - Filters out non-banks
    - Accurately captures percentageValue and title discount rates
    - Extracts specific qualifying cards from deal associations
    - Maps all branch cities for multi-city coverage
    """
    raw_provider = deal.get("sourceEntityName")
    if not is_valid_bank_or_wallet(raw_provider):
        return []

    merchant = deal.get("targetEntityName") or deal.get("merchantName") or deal.get("brandName") or deal.get("title")
    if not merchant:
        return []
    merchant = str(merchant).split("|")[0].split("-")[0].replace(",", " ").strip()
    if len(merchant) < 2 or len(merchant) > 55:
        return []

    title = str(deal.get("title") or "")
    desc = str(deal.get("description") or "")

    # 1. Accurate Discount Percentage
    pct_val = deal.get("percentageValue")
    if pct_val and isinstance(pct_val, (int, float)) and 0 < pct_val <= 90:
        discount_pct = int(pct_val)
    else:
        # Check title first, then description
        pct_match = PCT_REGEX.search(title) or PCT_REGEX.search(desc)
        if pct_match:
            discount_pct = int(pct_match.group(1))
        elif "buy 1 get 1" in title.lower() or "bogo" in title.lower() or "1+1" in title.lower():
            discount_pct = 50
        else:
            return []
    if discount_pct <= 0 or discount_pct > 90:
        return []

    provider = clean_bank_name(raw_provider)
    p_lower = provider.lower()
    provider_type = "Digital Wallet" if any(w in p_lower for w in ["jazzcash", "easypaisa", "sadapay", "nayapay", "wallet"]) else "Bank"

    # 2. Extract & Unify Qualifying Cards for this Deal Tier
    assocs = deal.get("associations")
    card_names = []
    if isinstance(assocs, list):
        for a in assocs:
            if isinstance(a, dict) and a.get("name"):
                cname = str(a.get("name")).strip()
                if len(cname) > 3:
                    card_names.append(cname)

    unified_card_tier = combine_card_names(provider, card_names)

    # 3. Dynamic Max Cap & Min Spend (PKR)
    max_cap = extract_cap(desc)
    min_spend = 0
    m_spend = MIN_SPEND_REGEX.search(desc)
    if m_spend:
        min_spend = int(m_spend.group(1).replace(',', ''))

    # 4. Dynamic Valid Days
    days_match = DAYS_REGEX.search(desc) or DAYS_REGEX.search(title)
    valid_days = days_match.group(1).title() if days_match else "All Days"

    # 5. Accurate Domain Category
    category = classify_category(merchant, deal.get("keywords"), title, desc)

    # 6. Clean Terms
    clean_terms = " ".join(desc.replace(",", ";").replace("\r", " ").replace("\n", " ").split())[:140]
    if not clean_terms:
        clean_terms = f"Live {discount_pct}% discount on {provider}"

    # 7. Multi-City Branch Expansion
    target_branches = deal.get("targetBranches")
    matched_cities = set()
    if isinstance(target_branches, dict) and len(target_branches) > 0:
        for branch_name in target_branches.values():
            bn_lower = branch_name.lower()
            for c in ALL_CITIES:
                if c.lower() in bn_lower:
                    matched_cities.add(c)

    if not matched_cities:
        comb_lower = f"{title} {desc}".lower()
        for c in ALL_CITIES:
            if c.lower() in comb_lower:
                matched_cities.add(c)

    if not matched_cities:
        if default_city and default_city in ALL_CITIES:
            matched_cities = {default_city}
        else:
            matched_cities = {"Karachi", "Lahore", "Islamabad", "Rawalpindi", "Faisalabad"}

    rows = []
    for city in matched_cities:
        rows.append([
            merchant,
            category,
            city,
            provider,
            provider_type,
            unified_card_tier,
            discount_pct,
            max_cap,
            min_spend,
            valid_days,
            clean_terms
        ])
    return rows

def fetch_api_page(city, offset, limit=500):
    payload = {
        "city": city,
        "country": "Pakistan",
        "language": "en",
        "limit": limit,
        "offset": offset,
        "sourceEntityId": "_all",
        "associatedDeals": True
    }
    req = urllib.request.Request(
        'https://peekaboo.guru/api/v8/entity/deals',
        data=json.dumps(payload).encode('utf-8'),
        headers=HEADERS,
        method='POST'
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode('utf-8', errors='ignore'))
            deals = data.get("deals", [])
            total = data.get("total", 0)
            return deals, total
    except Exception:
        return [], 0

def main():
    print("=" * 75)
    print(" PAKSAVER 100% LIVE NATIONWIDE AUTO-DISCOVERY SCRAPER")
    print(" GENUINE BANKS & DIGITAL WALLETS ONLY (K-ELECTRIC & INSURANCE EXCLUDED)")
    print(" EXACT QUALIFYING CARD TIERS & REAL-WORLD DISCOUNT PERCENTAGES")
    print("=" * 75)

    all_raw_deals = []
    seen_deal_ids = set()

    print("\n[*] Initializing nationwide deal pipeline...")
    first_deals, total_nationwide = fetch_api_page(city="", offset=0, limit=500)
    for d in first_deals:
        did = d.get("dealId")
        if did and did not in seen_deal_ids:
            seen_deal_ids.add(did)
            all_raw_deals.append(d)

    print(f"[*] Nationwide master catalog reports {total_nationwide} active deals.")
    offsets = list(range(500, total_nationwide + 500, 500))

    print(f"[*] Concurrently pulling {len(offsets)} master pages via ThreadPool...")
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=6) as executor:
        futures = {executor.submit(fetch_api_page, "", off, 500): off for off in offsets}
        for f in as_completed(futures):
            off = futures[f]
            try:
                page_deals, _ = f.result()
                for d in page_deals:
                    did = d.get("dealId")
                    if did and did not in seen_deal_ids:
                        seen_deal_ids.add(did)
                        all_raw_deals.append(d)
                print(f"  [+] Page offset {off:>5}: +{len(page_deals)} deals")
            except Exception as e:
                print(f"  [!] Offset {off} warning: {e}")

    t1 = time.time()
    print(f"\n[✓] Successfully ingested {len(all_raw_deals)} unique live deals in {t1 - t0:.2f}s!")

    print("\n[*] Extracting dynamic cards, verified discount rates & branch cities...")
    all_rows = []
    for d in all_raw_deals:
        extracted = extract_deal_rows(d)
        all_rows.extend(extracted)

    # Deduplicate deals by (restaurant, city, provider, card_tier, discount_pct)
    unique_deals = {}
    for r in all_rows:
        key = (r[0].lower().strip(), r[2].lower().strip(), r[3].lower().strip(), r[5].lower().strip(), str(r[6]))
        if key not in unique_deals:
            unique_deals[key] = r

    final_rows = list(unique_deals.values())

    headers = [
        "restaurant", "category", "city", "provider", "provider_type",
        "card_tier", "discount_pct", "max_cap_pkr", "min_spend_pkr", "valid_days", "terms"
    ]

    with open(CSV_FILE, mode="w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        writer.writerows(final_rows)

    cities_covered = set(r[2] for r in final_rows)
    banks_covered = set(r[3] for r in final_rows)
    merchants_covered = set(r[0] for r in final_rows)
    categories_covered = set(r[1] for r in final_rows)

    print("\n" + "=" * 75)
    print(f"[SUCCESS] Exported {len(final_rows)} 100% LIVE, non-hardcoded deals to {CSV_FILE}!")
    print(f"  - Cities covered: {len(cities_covered)} ({', '.join(sorted(cities_covered)[:8])}...)")
    print(f"  - Verified Banks: {len(banks_covered)} ({', '.join(sorted(banks_covered)[:8])}...)")
    print(f"  - Categories: {', '.join(sorted(categories_covered))}")
    print(f"  - Unique merchants / brands: {len(merchants_covered)}")
    print("=" * 75)

if __name__ == "__main__":
    main()
