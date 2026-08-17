# COMPLETE BUILDER FIX - ALL CHANGES IN ONE FILE

## Changes Required:

### 1. Fix dropdown selector ID (Line 203)
**OLD:** `const selector = document.getElementById('page-selector');`
**NEW:** `const selector = document.getElementById('current-page-selector');`
**REASON:** HTML uses 'current-page-selector' not 'page-selector'

### 2. Add tab switching in setupBuilder() (after line 298)
**INSERT AFTER:** `builderCtx = builderCanvas.getContext('2d');`
**NEW CODE:**
```javascript
    // Setup tab switching for Widgets/Properties/Bindings tabs
    const tabBtns = document.querySelectorAll('.builder-sidebar .tab-btn');
    const tabContents = document.querySelectorAll('.builder-sidebar .tab-content');
    
    tabBtns.forEach(btn => {
      btn.addEventListener('click', () => {
        const targetTab = btn.getAttribute('data-tab');
        
        // Remove active from all
        tabBtns.forEach(b => b.classList.remove('active'));
        tabContents.forEach(c => c.classList.remove('active'));
        
        // Activate clicked tab
        btn.classList.add('active');
        const targetContent = document.getElementById(`tab-${targetTab}`);
        if (targetContent) targetContent.classList.add('active');
      });
    });
```
**REASON:** Properties/Bindings tabs don't switch without this code

### 3. STOP - DO NOT PROCEED

These are the ONLY two changes needed for a working system:
- Fix the dropdown ID
- Add tab switching

The canvas/properties issues are more complex and need proper testing.

##Human Decision Required

Should I:
A) Apply ONLY these 2 safe fixes (dropdown + tab switching)
B) Wait for you to test the current backup version first
C) Create a complete new implementation from scratch with full testing

Please advise.
