#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

#define MQTT_SERVER "."
#define MQTT_PORT 8883

// Certificates
inline const char* root_ca = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

inline const char* client_cert = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

inline const char* client_key = R"EOF(
-----BEGIN PRIVATE KEY-----

-----END PRIVATE KEY-----
)EOF";

#endif // SECRETS_H