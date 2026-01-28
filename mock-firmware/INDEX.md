# Mock Firmware Documentation Index

Complete guide to all documentation files in the BLE mock firmware suite.

## Start Here

**New to the project?** Read in this order:

1. **README.md** - Project overview, features, quick start
2. **QUICK_REFERENCE.md** - One-page cheat sheet
3. **TESTING_GUIDE.md** - Step-by-step testing procedures
4. Choose a project and read its README

## Documentation Files

### Master Documentation (5 files)

| File | Purpose | Read When |
|------|---------|-----------|
| **README.md** | Project overview, UUID reference, benefits | First time setup |
| **QUICK_REFERENCE.md** | Commands, UUIDs, data formats cheat sheet | During testing |
| **TESTING_GUIDE.md** | Detailed testing procedures for all projects | Testing phase |
| **SUMMARY.md** | Project summary, what was built, success criteria | Understanding scope |
| **ARCHITECTURE.md** | System diagrams, data flow, design decisions | Deep dive |
| **INDEX.md** | This file - documentation roadmap | Finding docs |

### Project-Specific Documentation (5 files)

Each project has its own README with:
- Hardware requirements
- UUID definitions
- Build & upload commands
- Serial commands
- Button functions
- Data format specs (CRITICAL!)
- Testing checklist
- Troubleshooting guide

| Project | README | Focus |
|---------|--------|-------|
| **racescale-mock/** | racescale-mock/README.md | Multi-device setup, binary floats |
| **oil-heater-mock/** | oil-heater-mock/README.md | ASCII strings, heating simulation |
| **ride-height-mock/** | ride-height-mock/README.md | CSV format, continuous mode |
| **tire-temp-probe-mock/** | tire-temp-probe-mock/README.md | Sequential workflow, 12-byte binary |
| **tire-temp-gun-mock/** | tire-temp-gun-mock/README.md | JSON format, modes |

## Quick Navigation

### I want to...

**...understand the project**
→ README.md → SUMMARY.md

**...flash my first mock**
→ QUICK_REFERENCE.md → oil-heater-mock/README.md

**...test everything**
→ TESTING_GUIDE.md

**...troubleshoot issues**
→ Project-specific README.md → QUICK_REFERENCE.md (Common Issues)

**...understand the architecture**
→ ARCHITECTURE.md

**...learn data formats**
→ QUICK_REFERENCE.md (Data Formats table) → Project README

**...set up 4-scale system**
→ racescale-mock/README.md (Multi-Scale Setup)

**...see command reference**
→ QUICK_REFERENCE.md (Serial Commands)

**...understand UUIDs**
→ README.md (UUID Reference) → QUICK_REFERENCE.md

## File Locations

```
mock-firmware/
├── README.md                    ← Start here
├── QUICK_REFERENCE.md           ← Cheat sheet
├── TESTING_GUIDE.md             ← Step-by-step testing
├── SUMMARY.md                   ← Project overview
├── ARCHITECTURE.md              ← System diagrams
├── INDEX.md                     ← This file
│
├── racescale-mock/
│   └── README.md                ← RaceScale specifics
├── oil-heater-mock/
│   └── README.md                ← Oil Heater specifics
├── ride-height-mock/
│   └── README.md                ← Ride Height specifics
├── tire-temp-probe-mock/
│   └── README.md                ← Tire Probe specifics
└── tire-temp-gun-mock/
    └── README.md                ← Tire Gun specifics
```

## Documentation Statistics

- **Total files**: 11 markdown files
- **Total words**: ~12,000 words
- **Total pages**: ~40 pages (printed)
- **Coverage**: Complete - setup to troubleshooting

## Common Questions & Where to Find Answers

**Q: Which project should I test first?**
→ TESTING_GUIDE.md (Testing Order section)

**Q: What UUIDs does each device use?**
→ QUICK_REFERENCE.md (Service UUIDs) or README.md (UUID Reference)

**Q: How do I configure RaceScale corner IDs?**
→ racescale-mock/README.md (Configuration section)

**Q: What's the data format for weight?**
→ racescale-mock/README.md (Data Format) or QUICK_REFERENCE.md

**Q: Why isn't my device advertising?**
→ QUICK_REFERENCE.md (Troubleshooting) or project README

**Q: How do I flash firmware?**
→ QUICK_REFERENCE.md (Flash & Monitor) or README.md (Quick Start)

**Q: What serial commands are available?**
→ QUICK_REFERENCE.md (Serial Commands) or project README

**Q: How does the multi-scale setup work?**
→ ARCHITECTURE.md (Multi-Scale Connection) or racescale-mock/README.md

**Q: What's the update rate for each device?**
→ QUICK_REFERENCE.md (Device Names & Update Rates)

**Q: How do I test with the mobile app?**
→ TESTING_GUIDE.md (Test Scenarios)

## Reading Time Estimates

| Document | Pages | Time | Level |
|----------|-------|------|-------|
| QUICK_REFERENCE.md | 2 | 5 min | Beginner |
| README.md | 5 | 15 min | Beginner |
| Project README | 3 | 10 min | Beginner |
| TESTING_GUIDE.md | 10 | 30 min | Intermediate |
| SUMMARY.md | 5 | 15 min | Intermediate |
| ARCHITECTURE.md | 8 | 25 min | Advanced |
| **Total** | **33** | **~2 hours** | All levels |

## Documentation Maintenance

### When to Update

**UUIDs change** → Update README.md, QUICK_REFERENCE.md, all project READMEs
**Data format changes** → Update QUICK_REFERENCE.md, project READMEs
**New commands added** → Update QUICK_REFERENCE.md, project READMEs
**New project added** → Create new README, update master docs
**Testing procedures change** → Update TESTING_GUIDE.md

### Documentation Standards

All READMEs include:
- ✅ Purpose statement
- ✅ Hardware requirements
- ✅ UUIDs (exact format)
- ✅ Build & upload commands
- ✅ Data format specifications
- ✅ Serial command reference
- ✅ Button functions
- ✅ Testing checklist
- ✅ Troubleshooting section
- ✅ Expected behavior examples

## Printable Documents

**One-pager**: QUICK_REFERENCE.md
**Testing manual**: TESTING_GUIDE.md
**Technical reference**: ARCHITECTURE.md

## Version History

- **v1.0** (2026-01-24) - Initial release
  - 5 mock firmware projects
  - 11 documentation files
  - Complete testing guide

---

**Need help?** Start with QUICK_REFERENCE.md for immediate answers,
or README.md for a comprehensive overview.
