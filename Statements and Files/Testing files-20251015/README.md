# Testing Files

This directory contains all test materials for The Citadel System Operating Systems project, including house sigils and inventory databases for 14 different Game of Thrones houses.

## Contents Overview

- **14 House Sigils** (PNG images) - For testing file transfer and alliance visualization
- **14 Inventory Databases** (Binary files) - For testing product management and trading

## House Sigils (PNG Files)

Visual emblems representing each Great House. These files are used to test:
- File transfer protocol in Phase 2
- MD5SUM verification
- Sigil exchange during alliance formation (Phase 3)
- Display of allied Realm sigils

### Available Sigils

| House | File | Region |
|-------|------|--------|
| Arryn | [arryn.png](arryn.png) | The Vale |
| Baratheon | [baratheon.png](baratheon.png) | The Stormlands |
| Blackwood | [blackwood.png](blackwood.png) | The Riverlands |
| Cole | [cole.png](cole.png) | The Crownlands |
| Dustin | [dustin.png](dustin.png) | The North |
| Greyjoy | [greyjoy.png](greyjoy.png) | The Iron Islands |
| Hightower | [hightower.png](hightower.png) | The Reach |
| Lannister | [lannister.png](lannister.png) | The Westerlands |
| Peake | [peake.png](peake.png) | The Reach |
| Stark | [stark.png](stark.png) | The North |
| Strong | [strong.png](strong.png) | The Riverlands |
| Targaryen | [targaryen.png](targaryen.png) | The Crownlands |
| Tully | [tully.png](tully.png) | The Riverlands |
| Velaryon | [velaryon.png](velaryon.png) | The Crownlands |

### Usage Example

In `maester.dat` configuration:
```
Winterfell
8510
/path/to/stark.png
```

When testing alliance formation:
```bash
./maester winterfell.dat
> PLEDGE Casterly Rock
# On alliance acceptance, lannister.png should be transferred
```

---

## Inventory Databases (Binary Files)

Binary stock databases containing product information for each house. These files test:
- Binary file I/O operations
- Inventory loading and parsing (Phase 1)
- Product listing functionality
- Trading system (Phase 4)
- Inventory synchronization

### Available Inventory Files

| House | File | Purpose |
|-------|------|---------|
| Arryn | [arryn_stock.db](arryn_stock.db) | Vale products |
| Baratheon | [baratheon_stock.db](baratheon_stock.db) | Stormlands products |
| Blackwood | [blackwood_stock.db](blackwood_stock.db) | Riverlands products |
| Cole | [cole_stock.db](cole_stock.db) | Crownlands products |
| Dustin | [dustin_stock.db](dustin_stock.db) | Northern products |
| Greyjoy | [greyjoy_stock.db](greyjoy_stock.db) | Iron Islands products |
| Hightower | [hightower_stock.db](hightower_stock.db) | Reach products |
| Lannister | [lannister_stock.db](lannister_stock.db) | Westerlands products |
| Peake | [peake_stock.db](peake_stock.db) | Reach products |
| Stark | [stark_stock.db](stark_stock.db) | Northern products |
| Strong | [strong_stock.db](strong_stock.db) | Riverlands products |
| Targaryen | [targaryen_stock.db](targaryen_stock.db) | Dragon products |
| Tully | [tully_stock.db](tully_stock.db) | Riverlands products |
| Velaryon | [velaryon_stock.db](velaryon_stock.db) | Sea products |

### Binary File Format

Each inventory file uses the following binary structure:

**Record Size:** 64 bytes per product

**Structure:**
```c
typedef struct {
    char sProductName[50];  // Product name (null-terminated)
    int32_t nQuantity;      // Quantity in stock (little-endian)
    float fPrice;           // Price per unit (little-endian, IEEE 754)
    char sPadding[6];       // Padding to align to 64 bytes
} __attribute__((packed)) Product;
```

**Important Details:**
- All multi-byte integers use little-endian byte order
- Floating-point values use IEEE 754 standard
- Product names are null-terminated strings
- File may contain multiple 64-byte records
- Use `__attribute__((packed))` to prevent compiler padding

### Reading Binary Inventory

Example C code:
```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    char sProductName[50];
    int32_t nQuantity;
    float fPrice;
    char sPadding[6];
} __attribute__((packed)) Product;

void readInventory(const char* psFilePath) {
    FILE* pFile = fopen(psFilePath, "rb");
    if (!pFile) {
        perror("Failed to open inventory");
        return;
    }

    Product product;
    while (fread(&product, sizeof(Product), 1, pFile) == 1) {
        printf("Product: %s, Quantity: %d, Price: %.2f\n",
               product.sProductName,
               product.nQuantity,
               product.fPrice);
    }

    fclose(pFile);
}
```

### Inspecting Binary Files

Use command-line tools to examine file contents:

```bash
# View file in hexadecimal
hexdump -C stark_stock.db | head -20

# View file in octal/hex with addresses
od -x stark_stock.db | head -20

# Check file size (should be multiple of 64)
ls -l stark_stock.db
```

---

## Testing Scenarios

### Phase 1: Local Inventory Management

**Test Case 1:** Load Inventory
```bash
./maester winterfell.dat
> LIST PRODUCTS
# Should display all products from stark_stock.db
```

**Test Case 2:** Case-Insensitive Commands
```bash
> list products
> LIST products
> LiSt PrOdUcTs
# All variations should work
```

### Phase 2: File Transfer

**Test Case 3:** Sigil Transfer
```bash
# Terminal 1
./maester winterfell.dat
> PLEDGE Casterly Rock

# Terminal 2
./maester casterly_rock.dat
> PLEDGE RESPOND Winterfell y
# lannister.png should be sent to Winterfell
# stark.png should be sent to Casterly Rock
```

**Test Case 4:** MD5SUM Verification
- Transfer file successfully
- Verify MD5 checksum matches original
- Test corrupted transfer (intentional NACK scenario)

### Phase 3: Alliance Formation

**Test Case 5:** Alliance with Sigil Exchange
```bash
# Use multiple houses to test alliance network
# Verify sigil display for all allied Realms
> PLEDGE STATUS
# Should show sigils of allied houses
```

### Phase 4: Trading System

**Test Case 6:** Inter-Realm Trade
```bash
# Terminal 1 (Stark)
> START TRADE Lannister
trade> add "Obsidian" 100
trade> send

# Terminal 2 (Lannister)
# Accept or counter-offer
# Verify inventory updates in both stock.db files
```

---

## File Organization

```
Testing files-20251015/
├── README.md                 # This file
│
├── arryn.png                 # House Arryn sigil
├── arryn_stock.db            # House Arryn inventory
│
├── baratheon.png             # House Baratheon sigil
├── baratheon_stock.db        # House Baratheon inventory
│
├── blackwood.png             # House Blackwood sigil
├── blackwood_stock.db        # House Blackwood inventory
│
├── cole.png                  # House Cole sigil
├── cole_stock.db             # House Cole inventory
│
├── dustin.png                # House Dustin sigil
├── dustin_stock.db           # House Dustin inventory
│
├── greyjoy.png               # House Greyjoy sigil
├── greyjoy_stock.db          # House Greyjoy inventory
│
├── hightower.png             # House Hightower sigil
├── hightower_stock.db        # House Hightower inventory
│
├── lannister.png             # House Lannister sigil
├── lannister_stock.db        # House Lannister inventory
│
├── peake.png                 # House Peake sigil
├── peake_stock.db            # House Peake inventory
│
├── stark.png                 # House Stark sigil
├── stark_stock.db            # House Stark inventory
│
├── strong.png                # House Strong sigil
├── strong_stock.db           # House Strong inventory
│
├── targaryen.png             # House Targaryen sigil
├── targaryen_stock.db        # House Targaryen inventory
│
├── tully.png                 # House Tully sigil
├── tully_stock.db            # House Tully inventory
│
├── velaryon.png              # House Velaryon sigil
└── velaryon_stock.db         # House Velaryon inventory
```

---

## Creating Test Configurations

### Example Configuration Files

**winterfell.dat:**
```
Winterfell
8510
/path/to/Testing files-20251015/stark.png
```

**casterly_rock.dat:**
```
Casterly Rock
8511
/path/to/Testing files-20251015/lannister.png
```

**dragonstone.dat:**
```
Dragonstone
8512
/path/to/Testing files-20251015/targaryen.png
```

### Setting Up Test Environment

```bash
# Create configuration directory
mkdir -p test_configs

# Create multiple test Realms
cat > test_configs/stark.dat << EOF
Winterfell
8510
$(pwd)/Testing files-20251015/stark.png
EOF

cat > test_configs/lannister.dat << EOF
Casterly Rock
8511
$(pwd)/Testing files-20251015/lannister.png
EOF

# Copy inventory files to working directory
cp "Testing files-20251015/stark_stock.db" test_configs/stock.db
```

---

## Data Validation

### Expected Products Examples

Each house's inventory should contain thematic products:

**Northern Houses (Stark, Dustin):**
- Furs, Obsidian, Timber, Salted Fish, etc.

**Westerlands (Lannister):**
- Gold, Silver, Weapons, Fine Wines, etc.

**Iron Islands (Greyjoy):**
- Iron, Salt, Seafood, Ships, etc.

**Targaryen:**
- Dragon Eggs, Dragonglass, Ancient Scrolls, etc.

### Integrity Checks

After loading inventory, verify:
1. All product names are valid strings (null-terminated)
2. Quantities are non-negative integers
3. Prices are positive floats
4. File size is exact multiple of 64 bytes
5. No garbage data in padding bytes

---

## Common Issues and Solutions

### Problem: Binary File Won't Load
**Causes:**
- Incorrect byte order assumption
- Wrong struct alignment (missing `__attribute__((packed))`)
- File pointer not opened in binary mode (`"rb"`)

**Solution:**
```c
FILE* pFile = fopen(psFilePath, "rb");  // Note the "b" for binary
```

### Problem: Corrupted Product Names
**Causes:**
- Not checking null-termination
- Buffer overflow when reading

**Solution:**
```c
product.sProductName[49] = '\0';  // Force null-termination
```

### Problem: Wrong Quantity/Price Values
**Causes:**
- Endianness mismatch (reading big-endian as little-endian)

**Solution:**
- Ensure reading in little-endian format
- On big-endian systems, convert using byte swap

### Problem: Sigil File Not Found
**Causes:**
- Relative path in configuration
- File permissions

**Solution:**
```bash
# Use absolute paths in maester.dat
/Users/pabloramirezlazaro/Downloads/OS-Project/Statements and Files/Testing files-20251015/stark.png
```

---

## Additional Testing Tips

1. **Test with All Houses**
   - Don't just use one or two test files
   - Each house may have unique edge cases

2. **File Size Verification**
   ```bash
   ls -l *_stock.db
   # All should be multiples of 64 bytes
   ```

3. **Checksum Validation**
   ```bash
   md5sum stark.png
   # Compare before and after transfer
   ```

4. **Concurrent Testing**
   - Run multiple Maesters simultaneously
   - Test with 3+ Realms for multi-hop routing

5. **Error Injection**
   - Intentionally corrupt a frame's checksum
   - Test NACK handling
   - Simulate network timeouts

---

## Notes

- These test files are provided for validation and should not be modified
- Keep original files intact for consistent testing
- Create copies if you need to test inventory modifications
- Binary files are platform-independent (little-endian)
- PNG files are standard format, compatible with all image viewers

---

**Author:** Pablo Ramirez Lazaro
**Last Updated:** October 15, 2025
**Version:** 1.0

*"The citadel is made of knowledge, not stone." - Archmaester*
