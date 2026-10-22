# SENTINEL frontend + Firebase Realtime Database

This folder contains the two-tab SENTINEL website (Overview + Monitoring) and an optional Firebase Realtime Database listener.

## What works now
- Overview keeps the project introduction and conceptual system diagram.
- Monitoring contains the dashboard and animated working-model demo.
- When configured, the page listens to `sentinel/current` in Firebase Realtime Database and updates the dashboard fields as new values arrive.
- If Firebase is not configured, the existing demo UI remains available. Demo values are not real hardware measurements.

## Setup
1. In Firebase Console, open your project and create/use a **Realtime Database**.
2. Register a Web app in Project settings (</> icon) and copy its web configuration.
3. Edit `firebase-config.js` and replace every `PASTE_...` placeholder with your real project values. Set `databaseURL` to the exact URL shown in Firebase Console.
4. Ensure the database rules allow the intended authenticated read access. Do not make a production database publicly readable/writable just to make the page work.
5. Serve this folder from a local web server or host it (for example with Firebase Hosting). ES module imports may not work when opening `index.html` directly as a `file://` URL.
6. Have the backend/ESP32 write telemetry to the path `sentinel/current` using the schema below.

## Expected Realtime Database data shape

Path: `/sentinel/current`

```json
{
  "signalQuality": 82,
  "satellitesInView": 9,
  "accelerationG": 1.02,
  "gyroMagnitude": 0.08,
  "speedKmh": 18.4,
  "hdop": 0.9,
  "latitude": 16.49765,
  "longitude": 80.50024,
  "navigationStatus": "Available",
  "integrityAlert": "GNSS and IMU readings appear consistent",
  "updatedAt": 1792680000000
}
```

All fields are optional; if a field is missing, that displayed value is left unchanged. `updatedAt` can be a Unix timestamp in milliseconds or a display string.

## Important integration notes
- This is a **Realtime Database** implementation because it connects using a `databaseURL`. If your team uses Cloud Firestore instead, this listener must be changed.
- The frontend only reads telemetry. It does not send control commands to the ESP32.
- The moving car and signal lines are still a visual simulation. Live Firebase coordinates update the coordinate text; moving the car according to GPS coordinates requires a coordinate-to-map conversion and a suitable map/route.
- Firebase web configuration is not a private server credential, but database access must still be protected with proper Firebase Security Rules / Authentication.
- Before calling this a live hardware dashboard, verify that your backend/ESP32 is actually writing fresh measurements to the specified path.
