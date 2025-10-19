# Statements and Files

This directory contains all official project documentation, testing specifications, and test materials for The Citadel System Operating Systems project.

## Contents

### Project Documentation

#### [OS_Project_2025-26 v1.0_ICE.pdf](OS_Project_2025-26%20v1.0_ICE.pdf)
**Main Project Statement**

Complete project specification including:
- Project overview and theme (Game of Thrones universe)
- Four-phase development roadmap
- Detailed requirements for each phase
- Architecture descriptions (Maester and Envoy processes)
- Network protocol specifications
- File format definitions
- Command interface requirements
- Grading criteria and deadlines

**Key Dates:**
- Phase 1 (Valar Compilis): October 28, 2025 - 15%
- Phase 2 (Kingsroad): November 25, 2025 - 25%
- Phase 3 (Dance of the Sigils): December 16, 2025 - 30%
- Phase 4 (The Council of Envoys): January 13, 2026 - 30%

---

#### [Style Guide.pdf](Style%20Guide.pdf)
**C Programming Style Guide**

Mandatory coding standards for the project:
- File naming conventions (.c, .h, Makefile)
- Hungarian notation for variable naming
  - `n` for int
  - `f` for float
  - `c` for char
  - `s` for string
  - `p` for pointer
  - `g` for global
- Function naming: lowerCamelCase
- Indentation: 1 tab or 4 spaces
- Comment structure (file headers, function headers, inline)
- Brace placement rules
- Forbidden practices (goto, excessive globals)

**Important:** All submitted code must comply with this style guide.

---

#### [2025.10.06.Ports_ICE.pdf](2025.10.06.Ports_ICE.pdf)
**Port Assignment Documentation**

Contains port range assignments for network communication.

---

#### [PROJECT GROUPS 2025-26.pdf](PROJECT%20GROUPS%202025-26.pdf)
**Student Group Assignments**

Lists all project groups with assigned port ranges for testing and development.

Example entries include group members and their port allocations (e.g., 8510-8514).

---

### Testing Documentation

#### [OS_Project_2025-26 - testing Phase 1.pdf](OS_Project_2025-26%20-%20testing%20Phase%201.pdf)
**Phase 1 Testing Specification**

Test cases for local process management (Valar Compilis):

**Commands to Test:**
- `LIST REALMS` - Display known Realms
- `PLEDGE <realm>` - Request alliance
- `PLEDGE RESPOND <realm> <y/n>` - Accept/reject alliance request
- `LIST PRODUCTS` - Show inventory
- `START TRADE <realm>` - Begin trade session
- `PLEDGE STATUS` - Show alliance states
- `ENVOY STATUS` - Display Envoy information
- `EXIT` - Graceful shutdown

**Test Scenarios:**
- Configuration file parsing
- Binary inventory loading
- Command case-insensitivity
- Trade sub-menu operations (add, remove, send, cancel)
- Error handling for invalid commands
- Graceful process termination

---

#### [OS_Project_2025-26 - testing Phase 2.pdf](OS_Project_2025-26%20-%20testing%20Phase%202.pdf)
**Phase 2 Testing Specification**

Test cases for network communication (Kingsroad):

**Network Features to Test:**
- TCP socket creation and binding
- Frame serialization and deserialization
- Fletcher-16 checksum calculation and validation
- Multi-hop message routing (1-5 hops)
- ACK/NACK acknowledgment system
- File transfer protocol
- MD5SUM verification for transferred files
- Network discovery protocol
- Connection timeout handling
- Concurrent Envoy operations
- CTRL+C signal handling for graceful shutdown

**Test Scenarios:**
- Single-hop direct communication
- Multi-hop routing through intermediate Realms
- Checksum error detection (corrupted frames)
- Alliance acceptance/rejection over network
- Sigil file transfer integrity
- Graceful shutdown with active connections

---

### Test Materials

#### [Testing files-20251015/](Testing%20files-20251015/)
**Test Files Directory**

Contains all test files for validation. See [Testing files-20251015/README.md](Testing%20files-20251015/README.md) for details.

**Contents:**
- 14 house sigil images (PNG format)
- 14 inventory database files (binary format)

**Houses Included:**
- Arryn, Baratheon, Blackwood, Cole, Dustin
- Greyjoy, Hightower, Lannister, Peake
- Stark, Strong, Targaryen, Tully, Velaryon

---

## File Organization

```
Statements and Files/
├── README.md                                    # This file
├── OS_Project_2025-26 v1.0_ICE.pdf             # Main project statement
├── Style Guide.pdf                              # C programming standards
├── 2025.10.06.Ports_ICE.pdf                    # Port assignments
├── PROJECT GROUPS 2025-26.pdf                   # Group assignments
├── OS_Project_2025-26 - testing Phase 1.pdf    # Phase 1 tests
├── OS_Project_2025-26 - testing Phase 2.pdf    # Phase 2 tests
└── Testing files-20251015/                      # Test materials
    ├── README.md
    ├── arryn.png, baratheon.png, ...           # 14 sigil images
    └── arryn_stock.db, baratheon_stock.db, ... # 14 inventory files
```

---

## How to Use These Files

### For Development

1. **Start with the main project statement** ([OS_Project_2025-26 v1.0_ICE.pdf](OS_Project_2025-26%20v1.0_ICE.pdf))
   - Understand the complete project scope
   - Review phase requirements in order
   - Note deadlines and grading weights

2. **Review the style guide** ([Style Guide.pdf](Style%20Guide.pdf))
   - Set up your coding environment accordingly
   - Configure editor for proper indentation
   - Create code templates following conventions

3. **Check port assignments** ([2025.10.06.Ports_ICE.pdf](2025.10.06.Ports_ICE.pdf))
   - Identify your assigned port range
   - Configure maester.dat files with correct ports

### For Testing

1. **Use test files** from [Testing files-20251015/](Testing%20files-20251015/)
   - Copy sigil PNG files to test sigil exchange
   - Use inventory database files to test trading

2. **Follow test specifications:**
   - Phase 1: [OS_Project_2025-26 - testing Phase 1.pdf](OS_Project_2025-26%20-%20testing%20Phase%201.pdf)
   - Phase 2: [OS_Project_2025-26 - testing Phase 2.pdf](OS_Project_2025-26%20-%20testing%20Phase%202.pdf)

3. **Validate each requirement** before submission

---

## Key Information Quick Reference

### Phase Deadlines
| Phase | Name | Due Date | Weight |
|-------|------|----------|--------|
| 1 | Valar Compilis | Oct 28, 2025 | 15% |
| 2 | Kingsroad | Nov 25, 2025 | 25% |
| 3 | Dance of the Sigils | Dec 16, 2025 | 30% |
| 4 | The Council of Envoys | Jan 13, 2026 | 30% |

### File Formats

**Configuration File (maester.dat):**
```
<Realm_Name>
<Port_Number>
<Sigil_File_Path>
```

**Binary Inventory (stock.db):**
- Record size: 64 bytes
- Fields: Product name (50 bytes), Quantity (4 bytes), Price (4 bytes), Padding (6 bytes)

**Network Frame:**
- Total size: 320 bytes
- TYPE (1), ORIGIN (13), DESTINATION (13), DATA_LENGTH (1), DATA (256), RESERVED (34), CHECKSUM (2)

### Test Houses

Available in Testing files directory:
- **The North:** Stark, Dustin
- **The Vale:** Arryn
- **The Stormlands:** Baratheon
- **The Westerlands:** Lannister
- **The Iron Islands:** Greyjoy
- **The Riverlands:** Blackwood, Tully, Strong
- **The Reach:** Hightower, Peake
- **The Crownlands:** Cole, Velaryon, Targaryen

---

## Additional Notes

- All PDFs are official project documentation from the Operating Systems course
- Test files simulate real-world scenarios for Realm interactions
- Keep this directory intact for reference throughout development
- Report any discrepancies in documentation to course instructors

---

**Author:** Pablo Ramirez Lazaro
**Last Updated:** October 15, 2025
**Version:** 1.0
