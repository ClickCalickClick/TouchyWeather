#pragma once

// Launcher AppGlance: publishes the current temp + condition as the app's
// launcher subtitle (e.g. "72° Partly Cloudy"). Call on the way out of the
// app — foreground deinit and background-fetch exit — so the launcher always
// shows the freshest data we have without opening the app.
void glance_update(void);
