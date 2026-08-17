# DEBUG: Check Overlay JSON Structure

## Problem
LCD not showing custom colors despite builder showing them and API calls succeeding.

## Theory
The "color" field might not be in the JSON being sent by the builder, or it's being lost somewhere.

## Test Steps

### 1. Open Serial Monitor
- Open VS Code Terminal or PlatformIO serial monitor
- Set baud rate to **115200**

### 2. Change a Widget Color
- Open the builder
- Select the "Home" page (should have overlay loaded)
- Click on a widget (e.g., the header or a card)
- Change its color to RED (#ff0000)
- Click "Apply" button
- Watch the serial output

### 3. Check Serial Output
You should see something like:

```
[OVERLAY0] Full JSON:
{
  "base": 0,
  "widgets": [
    {
      "type": "weatherIcon",
      "x": 20,
      "y": 40,
      "w": 80,
      "h": 80,
      "color": "#ff0000",        <-- THIS IS IMPORTANT!
      ...
    }
  ]
}
  Widget: type=weatherIcon, color=#ff0000
```

### 4. Report What You See
Report the Serial output showing:
- Does each widget have a "color" field?
- What format is it? (e.g., "#ff0000" or null or missing?)
- Does it match what you set in the builder?

## Why This Matters
- If "color" is **MISSING**: Builder isn't sending it
- If "color" is **NULL**: JSON parsing isn't including it
- If "color" is present but LCD doesn't show it: Different rendering bug

## Next Steps
Based on what we find, we'll either:
1. Fix builder to send color field
2. Fix server to preserve color field
3. Fix rendering to use color correctly
4. Or combination of the above
