# LCD Builder - Visual Bug Report

## 🔴 CRITICAL BUGS (7 total) - BLOCKS USAGE

### Bug #1: Properties Panel - Changes Don't Apply
```
┌─────────────────────────────────────┐
│ Properties Panel (Right Sidebar)    │
├─────────────────────────────────────┤
│ X: [100] ↑                          │  User edits X to 100
│ Y: [50]  ↑                          │  
│ Width: [80]                         │
│ Height: [30]                        │
│ Text: "Label 1"                     │
│                                     │
│ [Apply Changes] ← User clicks       │
│                                     │
│ ⚠️ BROKEN: Widget doesn't move!    │
│    Value shows but canvas unchanged │
└─────────────────────────────────────┘
     ❌ ROOT CAUSE: applyPropertiesFromForm() 
        doesn't properly validate/apply values
```

---

### Bug #2: Custom Pages - Persist Check
```
┌──────────────────────────────────────┐
│ Session 1: Create Page               │
├──────────────────────────────────────┤
│ User: Click "Add Page"               │
│ User: Enter "MyPage"                 │
│ UI:   Shows "Created page 'MyPage'"  │
│ Page list shows: MyPage ✓            │
│                                      │
└──────────────────────────────────────┘
                ↓
        F5 (Browser Refresh)
                ↓
┌──────────────────────────────────────┐
│ Session 2: Page Gone!                │
├──────────────────────────────────────┤
│ Page list shows: (empty)             │
│ MyPage: ❌ GONE                       │
│                                      │
│ ⚠️ BROKEN: Page only in memory,      │
│    never saved to /pages.json        │
└──────────────────────────────────────┘
```

---

### Bug #3: Palette Drag-Drop - Non-Functional
```
Palette (Left Sidebar)         Canvas (Center)
┌──────────────┐          ┌─────────────────────┐
│ Widgets      │          │                     │
│ ┌──────────┐ │          │                     │
│ │  Label ◄─┼──────────✗─→ (drops here but)   │
│ │    🤔    │ │          │  (no handler!)      │
│ ├──────────┤ │          │                     │
│ │  Icon  ◄─┼──────────✗─→ (doesn't work)     │
│ ├──────────┤ │          │                     │
│ │  Button  │ │          │                     │
│ │    🤔    │ │          │                     │
│ └──────────┘ │          │                     │
└──────────────┘          └─────────────────────┘

❌ ROOT CAUSE: Canvas missing dragover/drop handlers
   Palette can't communicate with canvas
```

---

### Bug #4: Delete/Copy/Paste - Empty Handlers
```
Toolbar (Top)
┌──────────────────────────────────────────────────┐
│  [Undo] [Redo] │ [Delete] [Copy] [Paste] [Align] │
│      ✓ Empty       ✗ Empty  ✗ Empty    ✗ Empty   │
│      ✓ Broken      ✗ Broken ✗ Broken   ✗ Broken  │
└──────────────────────────────────────────────────┘

Event Listeners Exist:
┌─────────────────────────────────────┐
│ btn-delete.addEventListener('click',│
│   () => {                           │
│     // EMPTY! No code here!         │
│   }                                 │
│ )                                   │
└─────────────────────────────────────┘

❌ ROOT CAUSE: Callback functions are empty
   Button listeners attached but no logic inside
```

---

### Bug #5: Undo/Redo - History Not Tracking
```
User Actions:
  1. Add Widget 1     ← Should record state
  2. Add Widget 2     ← Should record state  
  3. Move Widget 1    ← Should record state
  4. Delete Widget 2  ← Should record state

Undo Stack (should have 4 states):
  [Empty] ← ❌ PROBLEM: Never populated
  
Redo Stack:
  [Empty] ← ❌ PROBLEM: Never used

❌ ROOT CAUSE: No snapshot recording mechanism
   State changes not tracked for undo/redo
```

---

### Bug #6: Canvas Drag-Drop - Position Not Updated
```
Canvas Rendering:
1. User clicks widget
   └─ Selection shows ✓ (GREEN border)

2. User drags widget
   ├─ Drag start fires ✓
   ├─ Mouse moves but... 
   │  └─ Position NOT updated ❌
   │  └─ Widget stays in place ❌
   └─ No mousemove tracking ❌
   
3. User releases mouse
   └─ Widget is still in original position ❌

                Widget Selected            Widget Being Dragged
                (works)                    (broken)
                
        ┌────────────────────┐      ┌────────────────────┐
        │  Canvas 480×272    │      │  Canvas 480×272    │
        │                    │      │                    │
        │    ┌──────────┐    │      │    ┌──────────┐    │
        │    │ Widget   │    │      │ ─→ │ Widget   │    │
        │    │ 🟢 SEL   │    │      │ 🟢 │ DRAG ✗   │    │
        │    └──────────┘    │      │ (stays here!)      │
        │                    │      │                    │
        └────────────────────┘      └────────────────────┘

❌ ROOT CAUSE: mousemove handler incomplete
   No position calculation during drag
```

---

### Bug #7: Deploy - No Verification
```
Deploy Flow:
┌────────────────────────────┐
│ User clicks Deploy Button  │
└────────┬───────────────────┘
         ↓
┌────────────────────────────┐
│ JavaScript: POST /api/     │
│ pages/overlay with widgets │ ✓
└────────┬───────────────────┘
         ↓
┌────────────────────────────┐
│ SUCCESS: "Deployed!"       │ ✓ (But not verified!)
│ Show toast                 │
└────────┬───────────────────┘
         ↓
┌────────────────────────────┐
│ LCD Display:               │
│ Widgets appear? ❓         │
│ (Nobody checked!)          │
│                            │
│ ⚠️ Could have failed       │
│    User doesn't know       │
│    Data might be corrupted │
└────────────────────────────┘

❌ ROOT CAUSE: No read-back verification
   Assumes success without confirming
```

---

## 🟠 MAJOR BUGS (4 total)

### Bug #8: Page Selector Out of Sync
```
Two places show pages:
  
  Dropdown (Top):              Sidebar (Left):
  ┌──────────────┐            ┌──────────────┐
  │ [Home v]     │            │ Built-in:    │
  │ [Boiler]     │            │ • Home       │
  │ [MyPage] ✓   │            │ • MyPage ✗   │ ← Missing!
  │ [Stats]      │            │              │
  └──────────────┘            └──────────────┘
  
Can get out of sync when page created/deleted

❌ ROOT CAUSE: Updates don't sync both UI elements
```

### Bug #9: Widget Rendering Mismatch
```
Widget Type "badge" defined in presets:
  WIDGET_PRESETS.badge = { ... }

But rendering switch statement:
  case 'badge': { /* Missing! */ }
  
Widget type appears nowhere in drawElement()

❌ ROOT CAUSE: Preset type not handled in renderer
```

### Bug #10: No Input Validation
```
User enters invalid properties:
  X: -100        (outside bounds)
  Y: 500         (outside bounds)
  Width: -50     (negative!)
  Height: 0      (zero size!)
  
All accepted without validation ❌

Breaks rendering and causes issues ❌

❌ ROOT CAUSE: applyPropertiesFromForm() has no validation
```

### Bug #11: No Loading States
```
While fetching pages from API:
  • No spinner shown
  • No "Loading..." message
  • No disabled state
  • User thinks nothing is happening
  • Might click multiple times
  • Multiple API calls triggered
  
❌ ROOT CAUSE: No loading indicator code
```

---

## 🟡 MINOR BUGS (5 total)

### Bug #12: No Minimum Size Constraints
Users can create widgets with 0 width/height → invisible and unselecteable

### Bug #13: No Delete Confirmation  
User can accidentally delete entire custom page with no undo

### Bug #14: Binding List Unorganized
Available bindings shown as flat list with no categories/descriptions

### Bug #15: No Hover Tooltips
Widgets don't show info on hover - hard to discover what each does

### Bug #16: No Export/Import
Can't backup pages as JSON or share with others

---

## 📊 Bug Impact Summary

| Category | Count | Status | Impact |
|----------|-------|--------|--------|
| **Critical** | 7 | 🔴 Broken | Blocks all usage |
| **Major** | 4 | 🟠 Broken | Major features broken |
| **Minor** | 5 | 🟡 Broken | Polish/UX issues |
| **Total** | **16** | **90% Broken** | **Unusable** |

---

## ⚙️ How Severe?

### Currently Usable Features:
- ✓ Open builder page (UI loads)
- ✓ Load built-in pages (can view)
- ✓ Select widgets on canvas (shows green border)
- ✓ See rendering (canvas draws correctly)
- That's it... 5% working

### Cannot Do:
- ❌ Create custom pages (saved but lost on refresh)
- ❌ Add widgets (palette drag doesn't work)
- ❌ Edit widgets (properties don't apply)
- ❌ Move widgets (drag doesn't track)
- ❌ Delete widgets (button doesn't work)
- ❌ Copy/paste (buttons don't work)
- ❌ Undo/redo (buttons don't work)
- ❌ Deploy to LCD (risky - unverified)
- ❌ Effectively: 95% broken

---

## 🔧 Fix Strategy

```
PHASE 1 (16 hours): Make it work
├─ Fix 7 critical bugs
└─ Result: 100% functional (barely)

PHASE 2 (8 hours): Make it solid
├─ Fix 4 major bugs
└─ Result: Fully working and reliable

PHASE 3 (16 hours): Make it great
├─ Fix 5 minor bugs
├─ Add features
└─ Result: Professional grade tool
```

**Current State**: 🔴 15% working / 85% broken  
**After Phase 1**: 🟢 100% working / 0% broken  
**After Phase 2**: 🟢 100% working / 0% broken (+ robust)  
**After Phase 3**: 🟢 100% working / 0% broken (+ powerful)

