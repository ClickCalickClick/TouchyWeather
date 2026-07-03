module.exports = [
  {
    "type": "heading",
    "defaultValue": "TouchyWeather Settings"
  },
  {
    "type": "text",
    "defaultValue": "Configure your weather app. Theme and units sync to the watch immediately."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Appearance"
      },
      {
        "type": "radiogroup",
        "messageKey": "Theme",
        "label": "Theme",
        "defaultValue": "0",
        "options": [
          { "label": "Light", "value": "0" },
          { "label": "Dark", "value": "1" }
        ]
      },
      {
        "type": "radiogroup",
        "messageKey": "TimeFormat",
        "label": "Time format",
        "defaultValue": "0",
        "options": [
          { "label": "Match watch",     "value": "0" },
          { "label": "12-hour (2 PM)",  "value": "1" },
          { "label": "24-hour (14:00)", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "AnimationsEnabled",
        "label": "Animations",
        "description": "Animate the weather icon and effects. Turn off to save battery — the icon shows a single static frame. (Animations also pause automatically a few seconds after you stop interacting.)",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Units"
      },
      {
        "type": "radiogroup",
        "messageKey": "Units",
        "label": "Measurement system",
        "defaultValue": "0",
        "options": [
          { "label": "Imperial (°F, mph)", "value": "0" },
          { "label": "Metric (°C, km/h)",  "value": "1" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "UseDewPoint",
        "label": "Show dew point",
        "description": "Replace the humidity reading on the main card with dew point temperature.",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Navigation"
      },
      {
        "type": "toggle",
        "messageKey": "LoopNavigation",
        "label": "Loop cards at edges",
        "description": "When on, pressing Up on the first card or Down on the last card wraps around the carousel. Turn off to exit the app at the edges, so you can use TouchyWeather as a Quick Launch replacement and leave with the hardware buttons.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SelectTogglesTheme",
        "label": "SELECT switches theme",
        "description": "When on, a short or long press of the SELECT (middle) button flips light/dark theme on ordinary cards. Turn off if you keep changing the theme by accident — theme is still switchable here in settings. (SELECT on the main card always refreshes the weather; on Radar it refreshes radar.)",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Background Updates"
      },
      {
        "type": "select",
        "messageKey": "BackgroundUpdateInterval",
        "label": "Auto-refresh interval",
        "description": "Fetch weather in the background even when the app is closed. May impact battery life. Requires firmware 4.0+.",
        "defaultValue": "3600",
        "options": [
          { "label": "Disabled (manual refresh only)", "value": "0" },
          { "label": "Every hour (recommended)", "value": "3600" },
          { "label": "Every 30 minutes", "value": "1800" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Location"
      },
      {
        "type": "input",
        "messageKey": "LocationOverride",
        "label": "Manual override",
        "description": "Optional. Format: lat,lon (e.g. 37.7749,-122.4194). Leave empty to use phone GPS.",
        "attributes": {
          "placeholder": "lat,lon",
          "limit": 32
        }
      },
      {
        "type": "toggle",
        "messageKey": "ShowLocation",
        "label": "Show location",
        "description": "Show the location name at the top of the main card.",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
