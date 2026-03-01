# Flutter Mobile Client

This app is a simple client for the C++ Crow API.

## Run

1. Install Flutter SDK.
2. Run:
   - `flutter pub get`
   - `flutter run`

## API URL

`PersonApiClient` defaults to `http://10.0.2.2:18080` (Android emulator).

If you run on iOS simulator or desktop, update `baseUrl` in `lib/main.dart` to match your API host.
