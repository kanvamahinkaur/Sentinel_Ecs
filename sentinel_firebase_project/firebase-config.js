// Copy your Firebase Realtime Database web app configuration here.
// Firebase config values identify your project; secure the database with Firebase Rules.
// Do not put service-account private keys in frontend code.
export const firebaseConfig = {
  apiKey: "AIzaSyC3s0nonk22rvKT-mGNGpCGHE1KakSULN8",
  authDomain: "sentinel-bc1f1.firebaseapp.com",
  databaseURL: "https://sentinel-bc1f1-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "sentinel-bc1f1",
  storageBucket: "sentinel-bc1f1.firebasestorage.app",
  messagingSenderId: "1028922757360",
  appId: "1:1028922757360:web:9fe2796aa7a76a47b97235",
};

// The ESP32/backend should write current telemetry at this path.
export const sentinelDataPath = "sentinel/current";
