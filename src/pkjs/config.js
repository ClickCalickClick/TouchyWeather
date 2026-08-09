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
      },
      {
        "type": "toggle",
        "messageKey": "BigMode",
        "label": "Big Mode",
        "description": "Accessibility mode for easier reading: much larger fonts, high-contrast colors, and simplified cards that show fewer, bigger items (the main card shows just the temperature and condition; forecast cards show fewer rows). The full detail is still available by pressing SELECT-and-hold (or swiping up) on a card.",
        "defaultValue": false
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
        "type": "select",
        "messageKey": "WindSpeedUnit",
        "label": "Wind speed",
        "defaultValue": "auto",
        "description": "Wind can use a different unit from the temperature. Automatic follows the measurement system above (mph with Imperial, km/h with Metric); metres per second is the everyday wind unit across Scandinavia and much of Europe.",
        "options": [
          { "label": "Automatic",           "value": "auto" },
          { "label": "Miles per hour (mph)", "value": "mph" },
          { "label": "Kilometres per hour (km/h)", "value": "kmh" },
          { "label": "Metres per second (m/s)",    "value": "ms" }
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
        "defaultValue": "Card Visibility"
      },
      {
        "type": "toggle",
        "messageKey": "PhoneManagesCards",
        "label": "Manage cards from phone",
        "description": "Off by default: show/hide and reorder cards on the watch (Settings card). Turn this ON to control card visibility and order from here instead — the toggles and the order list below then apply on save. The watch's Settings card keeps working either way; this page always opens showing the watch's current state.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledHours",
        "label": "6 Hours",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledWeek",
        "label": "Week Ahead",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledPrecip",
        "label": "Rain / Precipitation",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledUV",
        "label": "UV Index",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledAQ",
        "label": "Air Quality",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledSun",
        "label": "Sun Cycle",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledNight",
        "label": "Night Sky",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledGolden",
        "label": "Golden Hour",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledRadar",
        "label": "Radar",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "CardEnabledAdvice",
        "label": "Touch & Go (advice)",
        "defaultValue": true
      },
      {
        "type": "cardorder",
        "messageKey": "CardOrder",
        "label": "Card order",
        "description": "Drag the ≡ grip to reorder cards. Applied on save when 'Manage cards from phone' is on. The main weather card is always first and the Settings card always last. Order can also be changed on the watch (hold UP/DOWN on the Settings card).",
        "defaultValue": "9,0,1,2,3,4,5,6,7,8"
      },
      {
        "type": "toggle",
        "messageKey": "HideSettingsCard",
        "label": "Hide the Settings card on the watch",
        "description": "Remove the CARDS (Settings) card from the watch carousel, so scrolling is purely weather cards and all card management happens here. Only available while 'Manage cards from phone' is on — turning that off instantly brings the watch's Settings card back, so you can never lose card control.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "AutoHidePrecip",
        "label": "Auto-hide Rain & Radar when dry",
        "description": "When on, the Rain and Radar cards are automatically hidden while no rain is expected in the next few hours, and reappear the moment rain is forecast. Only hides cards you have enabled; it never overrides a card you turned off.",
        "defaultValue": false
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
