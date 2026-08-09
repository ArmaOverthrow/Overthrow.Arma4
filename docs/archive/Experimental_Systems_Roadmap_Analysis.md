# Experimental Systems & Overthrow Roadmap Analysis

## Executive Summary

Analysis of Overthrow's open GitHub issues reveals significant alignment between planned features and the new experimental systems in Arma Reforger. The Faction Commander system, deployable mechanics, and AI improvements directly address multiple high-priority roadmap items, potentially accelerating development of key features.

## Roadmap Categories

### Strategic Command Features
- **Issue #24: High Command** (v1.4.0 milestone)
- **Issue #22: AI Recruits** (v1.3.0 milestone)
- **Issue #11: Intel System** (v1.4.0 milestone)

### Building & Infrastructure
- **Issue #28: Mobile FOBs** (v1.3.0 milestone)
- **Issue #10: Building in Towns** (v1.4.0 milestone)
- **Issue #83: Vehicle Storage** (v1.3.0 milestone)

### Economy & Management
- **Issue #99: Economy 2.0**
- **Issue #51: Renaming Bases** (v1.3.0 milestone)

### AI & Gameplay Systems
- **Issue #71: AI Driving** (v1.3.0 milestone)
- **Issue #8: Undercover System** (v1.3.0 milestone)
- **Issue #100: Virtualization** (v1.3.0 milestone)

### UI & Usability
- **Issue #70: New Map System** (v1.4.0 milestone)
- **Issue #126: Custom Difficulty Settings**

## Direct System Matches

### 1. Faction Commander System → High Command (#24)

**Perfect Alignment**: The experimental Faction Commander system provides nearly all the infrastructure needed for High Command.

**What It Provides:**
- Command hierarchy structure
- Map-based waypoint assignment
- Group management interface
- Persistent command structure
- AI group control mechanisms

**What Overthrow Still Needs:**
- Barracks building integration
- Cost system for hiring groups
- Warehouse equipment integration
- Custom group/loadout designers

**Development Impact**: Could reduce High Command implementation time by 60-70%

### 2. Faction Commander + AI Systems → AI Recruits (#22)

**Strong Alignment**: The group cohesion and command systems provide foundation for recruit management.

**What It Provides:**
- Group assignment and control
- Individual unit management
- Waypoint and task systems
- Map-based command interface

**What Overthrow Still Needs:**
- Recruitment mechanics
- XP/leveling system
- Training camp functionality
- Equipment management for AI

**Development Impact**: Could accelerate AI Recruits by 40-50%

### 3. Deployable System → Mobile FOBs (#28)

**High Compatibility**: Multi-part deployable system is ideal for mobile FOB mechanics.

**What It Provides:**
- Deployment/undeployment mechanics
- Multi-component building system
- Placement validation
- State persistence

**What Overthrow Still Needs:**
- Vehicle-to-FOB conversion
- Officer permission checks
- Fast travel integration
- Enemy targeting priority

**Development Impact**: Could provide 50% of Mobile FOB functionality

### 4. Supply Allocation System → Economy 2.0 (#99)

**Moderate Alignment**: Supply allocation provides resource management framework.

**What It Provides:**
- Per-player resource tracking
- Faction-wide resource pools
- Distribution mechanics
- UI for resource management

**What Overthrow Still Needs:**
- Shop ownership system
- Import/export mechanics
- Product chains
- Economic data visualization

**Development Impact**: Could assist with 30-40% of Economy 2.0 features

### 5. Map Command Cursors → New Map System (#70)

**Good Foundation**: Map cursor system enhances strategic map interactions.

**What It Provides:**
- Interactive map commands
- Visual feedback system
- Position validation
- Command execution framework

**What Overthrow Still Needs:**
- Complete map overhaul
- Information layers
- Territory visualization
- Intel integration

**Development Impact**: Provides building blocks for 20-30% of new map features

## Indirect Benefits

### Intel System (#11)
- **AI Improvements**: Better patrol behaviors for intel gathering
- **Map Cursors**: Visual intel representation
- **Supply System**: Resource cost for intel operations

### Building in Towns (#10)
- **Deployable System**: Foundation for town construction
- **Placement Validation**: Building placement checks
- **Multi-part Construction**: Complex building assembly

### Undercover System (#8)
- **AI Danger Events**: Detection mechanics
- **Group Cohesion**: Civilian behavior patterns

### Vehicle Storage (#83)
- **Deployable Containers**: Storage mechanics
- **Persistence Framework**: Save/load improvements

## Priority Recommendations

### Phase 1: Immediate Integration (v1.3.0 targets)
1. **Mobile FOBs** - Leverage deployable system
2. **AI Recruits** - Use Faction Commander for control
3. **Vehicle Storage** - Adapt deployable containers

### Phase 2: Strategic Features (v1.4.0 targets)
1. **High Command** - Full Faction Commander integration
2. **New Map System** - Build on map cursor system
3. **Intel System** - Combine AI and map improvements

### Phase 3: Complex Systems
1. **Economy 2.0** - Extend supply allocation system
2. **Building in Towns** - Advanced deployable usage
3. **Undercover System** - AI behavior integration

## Risk Analysis

### Opportunities
- **Reduced Development Time**: 40-60% time savings on command features
- **Proven Systems**: Battle-tested code from base game
- **Network Optimization**: Efficient replication already implemented
- **UI Components**: Reusable interface elements

### Challenges
- **Integration Complexity**: Adapting systems to Overthrow's architecture
- **Feature Gaps**: Some unique requirements not covered
- **Update Dependencies**: Reliance on experimental branch stability
- **Learning Curve**: Understanding new system architectures

## Implementation Strategy

### Quick Wins (1-2 weeks each)
1. Prototype Faction Commander for basic High Command
2. Test deployable system for simple FOB mechanics
3. Implement basic AI group control for recruits

### Medium Term (3-4 weeks each)
1. Full High Command integration with barracks
2. Mobile FOB deployment system
3. Enhanced map with command cursors

### Long Term (5-8 weeks each)
1. Complete Economy 2.0 with supply allocation
2. Full AI Recruit system with progression
3. Comprehensive intel system

## Conclusion

The experimental systems provide substantial acceleration opportunities for Overthrow's roadmap, particularly for v1.3.0 and v1.4.0 milestone features. The Faction Commander system alone addresses multiple high-priority issues and could reduce overall development time by months.

**Key Recommendations:**
1. **Prioritize High Command** - Most direct benefit from new systems
2. **Fast-track Mobile FOBs** - Deployable system is ready-made
3. **Prototype AI Recruits** - Test group control immediately
4. **Plan Economy 2.0 Architecture** - Design around supply allocation

**Estimated Time Savings:**
- High Command: 2-3 months → 3-4 weeks
- AI Recruits: 6-8 weeks → 3-4 weeks  
- Mobile FOBs: 4-6 weeks → 2-3 weeks
- Total potential savings: 3-4 months of development time

The experimental systems represent a significant opportunity to accelerate Overthrow's development while improving quality through battle-tested base game systems.